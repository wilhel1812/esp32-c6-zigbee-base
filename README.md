# ESP32-C6 Zigbee Base

Reusable ESP-IDF Zigbee device foundation for the Waveshare ESP32-C6-LCD-1.3 board.

This repository owns the board support, Zigbee lifecycle, local visual states, and sectioned product config API used by product firmware.

## Example

```sh
cd examples/generic_device
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor
```

## Component Dependency

```yaml
dependencies:
  esp32_c6_zigbee_base:
    git: https://github.com/wilhel1812/esp32-c6-zigbee-base.git
    path: components/esp32_c6_zigbee_base
    version: v0.1.4
```

Product firmware should pin this component to exact tags, not branches. The repository is private, so the build environment must have GitHub access.

## Documentation

- Product config reference: `docs/config-reference.md`
- Generic device template: `docs/examples/generic-device.md`
- Versioning policy: `docs/development/versioning.md`
- Agent policy: `AGENTS.md`
