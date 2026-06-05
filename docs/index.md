---
title: Home
layout: home
nav_order: 1
---

# ESP32-C6 Zigbee Base

Reusable ESP-IDF Zigbee device foundation for the Waveshare ESP32-C6-LCD-1.3 board.

Product firmware should define one sectioned C product config and call `esp32_c6_zigbee_base_start_product()`. The config style is intentionally close to Klipper examples: identity first, board pins next, then feature sections.

## Start Here

- [Config Reference](config-reference.md)
- [Generic Device Example](examples/generic-device.md)
- [Versioning](development/versioning.md)
- [Agent Policy](development/agent-policy.md)
