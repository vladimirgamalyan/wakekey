#include "telegram.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#include "config.h"

static const char *TAG = "telegram";

// Long-poll: getUpdates is held open server-side for this long when idle, so
// the socket timeout must exceed it. limit=1 keeps each response to a single
// update, so the receive buffer stays small and the parse trivial.
#define TG_LONG_POLL_SECONDS 50
#define TG_HTTP_TIMEOUT_MS ((TG_LONG_POLL_SECONDS + 10) * 1000)
#define TG_BACKOFF_MS 5000

// A Telegram text message tops out at 4096 characters; 8 KiB leaves ample room
// for the surrounding JSON of one update.
#define TG_RX_CAPACITY 8192
#define TG_URL_CAPACITY 512

// Set by the polling task on an authorized /wake, drained by the main task,
// which owns all USB access. A flag is the only thing that crosses tasks.
static atomic_bool s_wake_requested;

// Response accumulator filled by the HTTP event handler across ON_DATA chunks.
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool overflow;
} tg_rx_t;

static esp_err_t tg_http_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
    tg_rx_t *rx = evt->user_data;
    if (rx->overflow) {
        return ESP_OK;
    }
    // Keep one byte for a NUL terminator so the buffer parses as a C string.
    if (rx->len + (size_t)evt->data_len >= rx->cap) {
        rx->overflow = true;
        return ESP_OK;
    }
    memcpy(rx->buf + rx->len, evt->data, evt->data_len);
    rx->len += (size_t)evt->data_len;
    return ESP_OK;
}

// Percent-encode src into dst for use in a URL query value. Unreserved
// characters (RFC 3986) pass through; everything else becomes %XX.
static void url_encode(const char *src, char *dst, size_t dst_cap)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 4 < dst_cap; ++si) {
        unsigned char c = (unsigned char)src[si];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[di++] = (char)c;
        } else {
            dst[di++] = '%';
            dst[di++] = hex[c >> 4];
            dst[di++] = hex[c & 0x0F];
        }
    }
    dst[di] = '\0';
}

// Fire-and-forget reply to the chat. The response is ignored; failures are
// logged and swallowed, since a lost acknowledgement must not stall polling.
static void tg_send_message(esp_http_client_handle_t client, int64_t chat_id, const char *text)
{
    char encoded[256];
    char url[TG_URL_CAPACITY];
    url_encode(text, encoded, sizeof(encoded));
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/sendMessage?chat_id=%lld&text=%s",
             config_telegram_token(), (long long)chat_id, encoded);
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sendMessage failed: %s", esp_err_to_name(err));
    }
}

// Match a bot command at the start of text, allowing the "/cmd@botname" form
// and a trailing argument, so "/wakeup" does not match "/wake".
static bool is_command(const char *text, const char *cmd)
{
    size_t n = strlen(cmd);
    if (strncmp(text, cmd, n) != 0) {
        return false;
    }
    char after = text[n];
    return after == '\0' || after == ' ' || after == '@';
}

static void tg_handle_command(esp_http_client_handle_t client, int64_t chat_id, const char *text)
{
    if (is_command(text, "/wake")) {
        ESP_LOGI(TAG, "Authorized wake from chat %lld", (long long)chat_id);
        // Flag the wake before the reply: the host waking must not wait on the
        // acknowledgement's round trip.
        atomic_store(&s_wake_requested, true);
        tg_send_message(client, chat_id, "Waking the host.");
    } else if (is_command(text, "/start")) {
        tg_send_message(client, chat_id, "RevRevRev online. Send /wake to wake the host.");
    }
    // Any other text from an authorized chat is intentionally ignored.
}

// Fast-forward *offset past the current backlog without acting on it, so a
// stale command from before boot cannot wake the host. getUpdates with
// offset=-1 returns only the most recent update; starting one past it discards
// everything older. Best effort: on any failure *offset is left untouched.
static void tg_prime_offset(tg_rx_t *rx, int64_t *offset)
{
    rx->buf[rx->len] = '\0';
    cJSON *root = cJSON_Parse(rx->buf);
    if (root == NULL) {
        return;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *last = cJSON_IsArray(result) ? cJSON_GetArrayItem(result, 0) : NULL;
    cJSON *update_id = (last != NULL) ? cJSON_GetObjectItem(last, "update_id") : NULL;
    if (cJSON_IsNumber(update_id)) {
        *offset = (int64_t)update_id->valuedouble + 1;
    }
    cJSON_Delete(root);
}

// Parse one getUpdates response, advancing *offset past every update seen so
// the same updates are not redelivered on the next poll.
static void tg_handle_response(esp_http_client_handle_t client, tg_rx_t *rx, int64_t *offset)
{
    rx->buf[rx->len] = '\0';
    cJSON *root = cJSON_Parse(rx->buf);
    if (root == NULL) {
        ESP_LOGW(TAG, "getUpdates JSON parse failed");
        return;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        ESP_LOGW(TAG, "Unexpected getUpdates payload");
        cJSON_Delete(root);
        return;
    }

    cJSON *update;
    cJSON_ArrayForEach(update, result) {
        cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(update_id)) {
            int64_t id = (int64_t)update_id->valuedouble;
            if (id >= *offset) {
                *offset = id + 1;
            }
        }

        cJSON *message = cJSON_GetObjectItem(update, "message");
        if (!cJSON_IsObject(message)) {
            continue;
        }
        cJSON *chat = cJSON_GetObjectItem(message, "chat");
        cJSON *text = cJSON_GetObjectItem(message, "text");
        if (!cJSON_IsObject(chat) || !cJSON_IsString(text)) {
            continue;
        }
        cJSON *chat_id_item = cJSON_GetObjectItem(chat, "id");
        if (!cJSON_IsNumber(chat_id_item)) {
            continue;
        }
        int64_t chat_id = (int64_t)chat_id_item->valuedouble;

        if (!config_chat_allowed(chat_id)) {
            ESP_LOGW(TAG, "Ignoring command from unauthorized chat %lld", (long long)chat_id);
            continue;
        }
        tg_handle_command(client, chat_id, text->valuestring);
    }

    cJSON_Delete(root);
}

static void telegram_task(void *arg)
{
    (void)arg;

    static char rx_buffer[TG_RX_CAPACITY];
    tg_rx_t rx = {
        .buf = rx_buffer,
        .cap = sizeof(rx_buffer),
    };

    esp_http_client_config_t cfg = {
        .url = "https://api.telegram.org",
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = TG_HTTP_TIMEOUT_MS,
        .event_handler = tg_http_event,
        .user_data = &rx,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        vTaskDelete(NULL);
        return;
    }

    char url[TG_URL_CAPACITY];
    int64_t offset = 0;

    // Drained once, before the first real poll, to skip any pre-boot backlog.
    // The same request path retries through the backoff until WiFi is up.
    bool primed = false;

    ESP_LOGI(TAG, "Polling Telegram getUpdates");
    while (1) {
        if (primed) {
            snprintf(url, sizeof(url),
                     "https://api.telegram.org/bot%s/getUpdates?timeout=%d&limit=1&offset=%lld",
                     config_telegram_token(), TG_LONG_POLL_SECONDS, (long long)offset);
        } else {
            snprintf(url, sizeof(url),
                     "https://api.telegram.org/bot%s/getUpdates?timeout=0&limit=1&offset=-1",
                     config_telegram_token());
        }
        esp_http_client_set_url(client, url);
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        rx.len = 0;
        rx.overflow = false;
        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            // Also the expected path until WiFi has an IP: back off and retry.
            ESP_LOGW(TAG, "getUpdates failed: %s (backing off)", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(TG_BACKOFF_MS));
            continue;
        }
        int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGW(TAG, "getUpdates HTTP %d (backing off)", status);
            vTaskDelay(pdMS_TO_TICKS(TG_BACKOFF_MS));
            continue;
        }
        if (rx.overflow) {
            // A truncated body cannot be parsed to learn its update_id, so
            // advance past the requested offset to avoid stalling on it. With
            // limit=1 and an 8 KiB buffer this is effectively unreachable.
            ESP_LOGW(TAG, "Response exceeded %u bytes, skipping update", (unsigned)rx.cap);
            offset += 1;
            continue;
        }
        if (!primed) {
            tg_prime_offset(&rx, &offset);
            primed = true;
            continue;
        }
        tg_handle_response(client, &rx, &offset);
    }
}

void telegram_start(void)
{
    xTaskCreate(telegram_task, "telegram", 8192, NULL, 5, NULL);
}

bool telegram_take_wake_request(void)
{
    return atomic_exchange(&s_wake_requested, false);
}
