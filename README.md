# ESP32-C6 Zigbee Base

Reusable ESP-IDF Zigbee device foundation for the Waveshare ESP32-C6-LCD-1.3 board.

This repository owns the board support and generic Zigbee lifecycle used by products:

- ESP32-C6 Zigbee Router commissioning and rejoin behavior
- BOOT/GPIO9 long-press factory reset
- ST7789 LCD status screen and GPIO22 backlight PWM
- GPIO8 WS2812 status LED
- Generic Mains Power Outlet endpoint
- Display backlight and status LED dimmable-light endpoints
- Identify, leave/reset, pairing, joined, rejoining, and error visual states

## Example

```sh
cd examples/generic_device
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor
```

## Component Dependency

For product firmware, consume the base component from this repository:

```yaml
dependencies:
  esp32_c6_zigbee_base:
    git: https://github.com/wilhel1812/esp32-c6-zigbee-base.git
    path: components/esp32_c6_zigbee_base
    version: v0.1.0
```

The base component manifest pulls the sibling board/display/LED components from
the same tag.

The repository is private, so the build environment must have GitHub access.
