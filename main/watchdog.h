#pragma once

/*
 * Liveness-watchdog thresholds for the Telegram poll loop (ADR-0008).
 *
 * The device is unattended and remote: when the network stack or the poll loop
 * wedges silently, nobody is present to power-cycle it, and its one job is to
 * work at the moment it is needed. The poll loop treats loss of contact with
 * Telegram as the failure signal and reboots (esp_restart) to recover.
 *
 * Two independent triggers, whichever fires first:
 *   - Wall-clock silence: the authoritative bound. Reboot when no getUpdates
 *     has succeeded within this window, regardless of how the failures arrive.
 *   - Consecutive failures: a fast path so a hard, rapidly-failing outage does
 *     not have to wait out the full silence window before restarting. Its
 *     wall-clock span varies with how quickly each request fails, which is why
 *     the silence bound above is the one that actually guarantees recovery.
 *
 * Both are deliberately generous: they must not trip on a routine WiFi
 * reconnect or a brief upstream blip. Tune them here.
 */

// Reboot after this many seconds without a successful getUpdates round trip.
#define TG_WATCHDOG_MAX_SILENCE_SECONDS 900

// Reboot after this many back-to-back failed poll cycles.
#define TG_WATCHDOG_MAX_CONSECUTIVE_FAILURES 100
