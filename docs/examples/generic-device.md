---
title: Generic Device
layout: default
parent: Examples
nav_order: 1
---

# Generic Device Example

The generic example lives in `examples/generic_device`. It uses a self-documented product config in `main/product_config.c` and a minimal `main.c` that starts the base from that config.

The example config is the template to copy for simple products:

- Update the required identity fields.
- Set a real `product_url`.
- Choose the correct `generic_device_type`.
- Confirm board pins.
- Keep endpoint IDs unique.
- Disable feature sections that do not apply.

Future product examples, such as a thermostat, should follow the same section order and only add product-specific sections once the base supports that behavior.
