# 0008. Reboot the device when Telegram contact is lost

- Status: Accepted
- Date: 2026-07-22

## Context

The device is unattended and remote: it sits in a USB port on a machine the
owner is away from, and its one job is to work at the moment `/wake` is sent. If
the firmware wedges silently — the WiFi/TLS stack stuck in a state it does not
recover from, the poll loop no longer making progress — the device becomes
unreachable, and there is nobody present to power-cycle it. A silent, undetected
failure is the worst outcome this product has, because it surfaces only when the
wake is needed and does not come.

ESP-IDF already ships two watchdogs, and they leave a gap:

- The **interrupt watchdog** (`CONFIG_ESP_INT_WDT`) reboots on a hard CPU lock —
  interrupts disabled too long, a deadlock in a critical section. This class is
  covered.
- The **task watchdog** (`CONFIG_ESP_TASK_WDT_EN`) is enabled but toothless for
  this failure: it runs in non-panic mode (it only logs, never reboots) and
  watches only the idle tasks. When the `telegram` task is blocked on a socket
  or spinning through the backoff, the idle task still runs, so nothing trips.

What neither covers is a poll loop that has stopped making *progress* without
hard-locking the CPU: contact with Telegram is lost, but the chip is otherwise
healthy. That is precisely the failure that matters here.

Two options were weighed for closing it:

- **Enable task-watchdog panic and subscribe the `telegram` task.** Reuses the
  built-in mechanism, but it reboots on task *lateness*, not on the thing we
  actually care about. The task legitimately blocks up to ~60 s inside a single
  long-poll `getUpdates`, so the 5 s timeout would have to be retuned above that
  to avoid false reboots, and a change to `sdkconfig` panic behaviour affects
  every task, not just this one.
- **An application-level liveness watchdog keyed on successful `getUpdates`.**
  A completed round trip to Telegram (HTTP 200) is the device's own definition
  of "working". Losing it for too long triggers `esp_restart()`. This targets
  the product's failure directly, needs no `sdkconfig` change, and also catches
  persistent-error loops — WiFi associated but the stack wedged returning errors
  forever — that the task watchdog's idle-task check never sees.

## Decision

Add a liveness watchdog inside the Telegram poll loop. Each completed round trip
(HTTP 200) records a success; each failed poll cycle records a failure. The
device calls `esp_restart()` when either bound is crossed:

- **Wall-clock silence** — no successful `getUpdates` within
  `TG_WATCHDOG_MAX_SILENCE_SECONDS` (15 min). This is the authoritative bound; it
  holds regardless of how the failures arrive.
- **Consecutive failures** — `TG_WATCHDOG_MAX_CONSECUTIVE_FAILURES` (100)
  back-to-back failed cycles, a faster path so a hard, rapidly-failing outage
  does not have to wait out the full silence window.

Both thresholds live in `main/watchdog.h` as the single tuning surface, and both
are deliberately generous so a routine WiFi reconnect or a brief upstream blip
does not trip them. The task-watchdog `sdkconfig` is left as ESP-IDF ships it.

## Consequences

Easier:

- A silently wedged network stack or poll loop self-heals unattended, without
  anyone power-cycling the device.
- No new hardware, no external supervisor, and no change to the shared
  task-watchdog panic behaviour — the recovery policy is one small, self-
  contained piece keyed on the one signal that defines "working".
- The reboot policy is tunable in one documented header.

Harder:

- A genuine long upstream outage (WiFi down, ISP down) also trips the watchdog
  and reboots. This is harmless — during an outage no commands are being served
  anyway — but the reboot re-primes `offset=-1`, so a command that arrives in
  the ~2 s reboot window would be dropped rather than acted on. Thresholds are
  set generously to keep this rare.
- The watchdog is evaluated only when the loop iterates, so a call that blocks
  *forever* inside the network stack escapes both bounds. This is not observed
  and the socket timeout bounds the common stuck-recv case; if such a hang ever
  appears, subscribing the task to the task watchdog with panic enabled remains
  an available follow-up. The interrupt watchdog still covers hard CPU locks.
- Uptime resets on a watchdog reboot, so `/status` uptime is only ever "time
  since the last successful contact was lost", not lifetime.
