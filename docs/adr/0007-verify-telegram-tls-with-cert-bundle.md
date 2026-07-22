# 0007. Verify Telegram TLS with the mbedTLS certificate bundle

- Status: Accepted
- Date: 2026-07-22

## Context

The device polls `api.telegram.org` over HTTPS ([ADR-0002](0002-poll-telegram-directly.md)),
so it must decide how to authenticate the server certificate. Two options were
weighed:

- **Pin a certificate** — embed Telegram's leaf or intermediate CA and trust
  only it. Strictest, but brittle: it breaks the moment Telegram rotates to a
  certificate chaining through a different CA, taking every deployed device
  offline until reflashed. For a device whose whole job is to work at the moment
  it is needed, a silent expiry tied to someone else's rotation schedule is the
  worst failure mode there is.
- **The mbedTLS certificate bundle** — ESP-IDF ships a bundle of common root CAs
  (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`, already enabled, full list, 200 certs),
  attached to a TLS session with `esp_crt_bundle_attach`. The server certificate
  is validated against whichever public root it chains to, so CA rotation within
  the bundle is transparent.

The bundle's cost is a wider trust base: the device trusts any server presenting
a certificate from any CA in the bundle, not Telegram's specifically. That
matters when an attacker can both intercept traffic and obtain a valid
certificate for `api.telegram.org` from any trusted CA. Against this device's
threat model — waking a home computer over the public Telegram API — that is not
a meaningful exposure, and it is the same trust model every ordinary HTTPS
client on the network already runs under.

## Decision

Validate the Telegram TLS connection against the ESP-IDF mbedTLS certificate
bundle via `esp_crt_bundle_attach`. Do not pin a Telegram certificate.

## Consequences

Easier:

- Telegram can rotate certificates freely as long as the chain terminates in a
  root in the bundle; no reflash, no coordinated update.
- No certificate material to embed, track, or refresh in the build.

Harder:

- The trust base is every CA in the bundle, not Telegram's chain alone. A
  mis-issued certificate for `api.telegram.org` from any bundled CA would be
  accepted.
- The bundle is only as current as the pinned ESP-IDF version; a CA the bundle
  omits (or an updated bundle that drops one Telegram later relies on) surfaces
  as a validation failure at the next IDF bump. If stricter assurance is ever
  wanted, pinning can be reconsidered in a follow-up ADR.
