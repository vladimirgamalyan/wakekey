#include "config.h"

#include <stddef.h>

#include "secrets.h"

const char *config_wifi_ssid(void)
{
    return WIFI_SSID;
}

const char *config_wifi_password(void)
{
    return WIFI_PASSWORD;
}

const char *config_telegram_token(void)
{
    return TELEGRAM_BOT_TOKEN;
}

bool config_chat_allowed(int64_t chat_id)
{
    static const int64_t allowed[] = TELEGRAM_ALLOWED_CHAT_IDS;
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
        if (allowed[i] == chat_id) {
            return true;
        }
    }
    return false;
}
