# 0001. Use USB HID to wake the host

- Status: Accepted
- Date: 2026-07-16

## Context

The device must wake a sleeping computer by emulating a keypress. The ESP32-S3
can present itself as an HID keyboard over either of two transports:

- **Native USB** — the S3's USB-OTG peripheral, driven through TinyUSB.
- **BLE** — a Bluetooth Low Energy HID peripheral.

The host decides whether an HID event is allowed to wake it, and the two
transports differ sharply in how consistently that works. USB wake is a
long-established path: it is exposed in essentially every BIOS/UEFI and in the
per-device power settings of every desktop OS. Bluetooth wake depends on the
host's BT controller remaining powered in sleep, on its firmware honoring the
wake, and on the OS having paired the device beforehand — support is uneven
across machines and often absent on desktops.

Waking is the entire point of the product. A transport that works on some hosts
and not others fails the core requirement.

## Decision

Use USB HID over the native USB peripheral as the only wake transport. Do not
implement a BLE HID path.

## Consequences

Easier:

- Wake works on the broad majority of hosts, gated only by settings the user can
  find and change.
- No pairing, no bonding state to lose, no radio stack in the firmware.
- The device is powered by the port it is plugged into — no battery, no charging.
- One transport means one code path.

Harder:

- The device must be physically plugged into the machine it wakes; it is
  tethered and cannot be moved or shared between hosts.
- Requires a USB port that keeps power in the target sleep state. On many
  desktops only a subset of ports qualifies, and this is not discoverable from
  the device side — the user must find a working port.
- No recourse on hosts where USB wake is disabled in firmware and the setting is
  locked.
