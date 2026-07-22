#pragma once

#include <stdbool.h>

/*
 * Telegram command path (ADR-0002): a background task long-polls the Bot API
 * over HTTPS and flags a wake request when an authorized chat sends /wake.
 *
 * The task never touches USB itself. All USB access stays in the main task, so
 * the task signals through a flag that the main loop drains and acts on.
 */

// Start the background polling task. Call once, after WiFi has been brought up.
void telegram_start(void);

// Return true at most once per authorized /wake, clearing the request. Polled
// by the main loop, which owns the USB HID wake.
bool telegram_take_wake_request(void);
