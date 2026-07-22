# RevRevRev

Wake a sleeping computer from Telegram.

An ESP32-S3 sits in a USB port and presents itself to the host as an ordinary
keyboard. Send `/wake` to a Telegram bot and the device emits a keypress — the
host wakes exactly as it would from a real key. No agent or driver runs on the
host, and nothing runs on it while it sleeps.

The name is the reveille wake-up call, "Rev" three times — waking a sleeping host
is the device's only job ([ADR-0005](docs/adr/0005-rename-to-revrevrev.md)).

## How it works

1. The device joins WiFi and long-polls the Telegram Bot API over HTTPS.
2. You send `/wake` from a chat on its allowlist.
3. The device emits a USB HID keypress and the host wakes.

Because the host sees a plain USB keyboard, this works on any OS with no
host-side software. See [docs/DESIGN.md](docs/DESIGN.md) for how it works in
depth.

## Getting started

You need [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v5.x and an
ESP32-S3 board with native USB (USB-OTG, not a USB-to-UART bridge).

1. Copy the credentials template and fill it in:
   ```sh
   cp main/secrets.h.example main/secrets.h
   ```
   Set your WiFi credentials, the bot token from [@BotFather](https://t.me/BotFather),
   and your Telegram chat ID (message the bot and read `chat.id`, or use
   [@userinfobot](https://t.me/userinfobot)).
2. Build and flash:
   ```sh
   idf.py build
   idf.py -p PORT flash
   ```
3. Send `/wake` to your bot.

The host must also permit USB wake in BIOS and the OS — see
[Host requirements](docs/DESIGN.md#host-requirements).

## Status

Working prototype: the device wakes the host on `/wake` from an allowlisted chat.
The main remaining work is runtime provisioning, to get credentials out of the
firmware build. The risk-ordered plan is in [ROADMAP.md](ROADMAP.md).

## Security

Credentials are compiled into the firmware
([ADR-0004](docs/adr/0004-compile-time-secrets-header.md)), so do not publish
built images or build them in CI. A Telegram bot is reachable by anyone who knows
its username, so the chat allowlist is the only access control — keep it tight.
Treat the device as physically trusted hardware. Details in
[Security](docs/DESIGN.md#security).

## Documentation

- [docs/DESIGN.md](docs/DESIGN.md) — how the firmware works, in depth.
- [docs/adr/](docs/adr/) — the decisions behind it, and why.
- [ROADMAP.md](ROADMAP.md) — what is done and what is next.

## License

MIT — see [LICENSE](LICENSE).
