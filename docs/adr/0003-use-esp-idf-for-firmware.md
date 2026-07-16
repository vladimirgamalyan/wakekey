# 0003. Use ESP-IDF (C) for the firmware

- Status: Accepted
- Date: 2026-07-16

## Context

The firmware needs three things: USB HID via the S3's native USB peripheral
(ADR-0001), HTTPS with TLS to reach the Telegram API (ADR-0002), and persistent
storage for credentials.

The realistic options are ESP-IDF (Espressif's native SDK, C), Arduino with
PlatformIO, and MicroPython. Arduino would reach a first prototype faster, with
existing USB HID and Telegram bot libraries. But all three requirements sit at
the layer Arduino abstracts away — USB descriptors, the TLS stack, and NVS — and
this device's job is to be a convincing keyboard to a machine that is asleep.
That is precisely where descriptor-level and power-behavior control matters.

## Decision

Write the firmware in C against ESP-IDF.

## Consequences

Easier:

- First-class access to TinyUSB, mbedTLS, and NVS, with USB descriptors and
  remote-wakeup behavior under our direct control.
- Espressif supports the S3 in IDF first; board support and errata fixes arrive
  here before they reach wrappers.
- Explicit control over tasks, memory, and power.

Harder:

- More boilerplate and a steeper ramp than Arduino; slower to a first prototype.
- No ready-made Telegram bot library — HTTP requests, JSON parsing, and update
  offsets are ours to write and maintain.
- A smaller pool of copy-paste examples than the Arduino ecosystem.
