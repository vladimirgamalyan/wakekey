# Roadmap

Ordered by risk rather than by convenience. Waking a sleeping host over USB is
the assumption the whole product rests on, and it depends on the host and the
board — not on us. Nothing involving Telegram gets built until it is proven,
because if the host will not wake, every line of it is wasted.

## 1. Prove the host wakes at all — no code

**Done, 2026-07-16.** A plain USB keyboard wakes the target host from sleep, in
any port, with nothing changed in BIOS or Device Manager — the settings were
already permissive by default. No port restriction to design around, so the
device can live wherever it is convenient.

The procedure is kept below, because other hosts will not necessarily be this
accommodating: plug an ordinary USB keyboard into the port the device will
occupy, sleep the machine, press a key. If it does not wake, the settings to
work through are `Wake on USB` / `USB Power in S3/S4` in BIOS/UEFI, `ErP` or
`Deep Sleep` (must be off), and the per-device *Allow this device to wake the
computer* checkbox in Windows Device Manager.

What this establishes: wake over USB is permitted by the host firmware, and the
ports keep power in the sleep state used. What it does not establish is covered
by step 2 — a keyboard and this board are not the same load, and not the same
device asking to wake.

## 2. Firmware spike — wake over USB HID

**Done, 2026-07-21.** A BOOT button press wakes this host from sleep. A
suspended bus can only be woken through `tud_remote_wakeup()`, and that call is
a no-op unless the host has enabled remote wakeup — so the wake clears both
concerns below on this host: it granted remote wakeup to this board (not a
keyboard), and it tolerated the board's out-of-spec suspend current without
cutting port power. That spike ran with WiFi off; a follow-up spike (2026-07-22)
then held the port with WiFi up, no power save, and forced periodic TX through
an overnight S3 sleep — no brownout, no reset (the onboard LED stayed green
throughout), and BOOT still woke the host. So even the WiFi-up suspend load, the
design's knowingly out-of-spec case, is tolerated on this host. (The host did
wake once overnight, but `powercfg -lastwake` pinned that on its own Wi-Fi
card's Wake-on-WLAN, not the device.)

An ESP32-S3 that enumerates as an HID keyboard and triggers a wake on a BOOT
button press. No WiFi, no TLS, no Telegram.

A real keyboard and this board fail in different ways, so step 1 does not cover
this:

- **Remote wakeup is its own mechanism, not just HID.** The configuration
  descriptor must carry the remote wakeup attribute, the host must enable it
  (observable in `tud_suspend_cb`), and the wake must go through
  `tud_remote_wakeup()` — the bus is suspended, so an ordinary HID report will
  not do.
- **Suspend current.** USB budgets a suspended device roughly 2.5 mA. An
  ESP32-S3 with WiFi up draws orders of magnitude more, so the design is
  knowingly out of spec. Most hosts ignore this; some cut port power or report
  over-current. Only the actual hardware can answer which kind we have.

**Done when:** this board wakes this host from sleep.

## 3. Telegram command path

**Done, 2026-07-22.** The device long-polls `getUpdates` over HTTPS — TLS
verified against the mbedTLS certificate bundle
([ADR-0007](docs/adr/0007-verify-telegram-tls-with-cert-bundle.md)) rather than a
pinned cert — parses updates with cJSON, and wakes on `/wake` from an allowlisted
chat, replying with an acknowledgement. Verified live: an authorized `/wake`
drove the wake action and its Telegram reply, with an onboard-LED blink as local
confirmation. Two things are proven only at code level so far: the wake fired to
an *awake* host, so the from-sleep leg still rests on step 2 — it runs the same
`tud_remote_wakeup()` path proven there; and the unauthorized-chat rejection
rests on the allowlist in `config_chat_allowed()`, not yet a second-account test.
Credentials read through the single accessor in `config.c`
([ADR-0004](docs/adr/0004-compile-time-secrets-header.md)), and the backlog is
drained on boot (`offset=-1`) so a stale command cannot wake the host after a
reboot.

WiFi, then HTTPS long polling of `getUpdates` ([ADR-0002](docs/adr/0002-poll-telegram-directly.md)).

The chat ID allowlist belongs here and is not optional — the bot is reachable by
anyone who knows its username, so the allowlist is the only access control there
is. Credentials go behind a single narrow accessor, per
[ADR-0004](docs/adr/0004-compile-time-secrets-header.md).

**Done when:** a message from an authorized chat wakes the machine, and one from
any other chat does not.

## 4. Runtime provisioning

Replaces the compile-time header with SoftAP/BLE provisioning, superseding
[ADR-0004](docs/adr/0004-compile-time-secrets-header.md) via a new ADR.

Until this lands, credentials are compiled into the firmware, which means
firmware images must never be published or built in CI. The repository is
public, so that constraint is live and easy to trip over — which is the argument
for not deferring this indefinitely.

## Open questions

- Which ESP32-S3 module.
- **Does the host sleep in S3, or in Modern Standby (S0ix)?** Answered
  2026-07-22: S3. `powercfg /a` reports Modern Standby unavailable, so the
  suspend-current concern was the live kind, not the sort that evaporates under
  S0ix — and the step 2 note records that the port tolerated it anyway, WiFi up.
  Other hosts still need this check; the answer changes what they face.
