# 0002. Poll the Telegram Bot API directly from the device

- Status: Accepted
- Date: 2026-07-16

## Context

Commands must reach the device from Telegram. The device sits on a home network
behind NAT with no public IP and no inbound reachability.

The Telegram Bot API offers two delivery modes:

- **Webhook** — Telegram pushes updates to a public HTTPS endpoint. Unusable
  directly: it requires an inbound-reachable address and a valid certificate.
- **Long polling (`getUpdates`)** — the client opens an outbound HTTPS request
  that Telegram holds until an update arrives. Works from behind NAT.

That leaves two viable designs: the device polls `api.telegram.org` itself, or
we run a relay on a VPS that receives a webhook and forwards commands to the
device over a persistent channel (MQTT or WebSocket).

The relay's main attraction is that the bot token stays off the device. Its cost
is a server we must run, pay for, secure, and keep alive — and it becomes a
second thing that can break in the wake path, for a project whose entire job is
to work reliably at the moment it is needed.

## Decision

The device polls `api.telegram.org` directly over HTTPS long polling. No relay,
no server component.

## Consequences

Easier:

- No server to operate, pay for, or maintain; no public IP, no port forwarding,
  no certificate to renew.
- Fewer moving parts in the wake path — the device and Telegram, nothing else.
- Deployment is flashing a board and joining WiFi.

Harder:

- The bot token and WiFi credentials live on the device in NVS. Physical access
  to the device potentially exposes both. See the security notes in the README.
- Rotating the token means reconfiguring the device, not editing a server.
- The device must maintain WiFi and a TLS session while the host sleeps, and do
  its own HTTP and JSON handling.
- Networks that block Telegram block the product; there is no relay to route
  around it.
- Access control is the device's job alone: the bot is reachable by anyone who
  knows its username, so the allowlist of permitted chat IDs is the only thing
  standing between a stranger and the wake button.
