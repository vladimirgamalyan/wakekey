#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Narrow accessor over device credentials. Today these read from the
 * compile-time secrets header (ADR-0004); routing every read through this one
 * interface is what lets runtime provisioning (ADR-0004's intended successor)
 * replace the bodies here rather than spread through the codebase.
 */

const char *config_wifi_ssid(void);
const char *config_wifi_password(void);
const char *config_telegram_token(void);

// True if chat_id is on the allowlist permitted to wake the host. The bot is
// reachable by anyone who knows its username, so this is the only access
// control there is (ADR-0002).
bool config_chat_allowed(int64_t chat_id);
