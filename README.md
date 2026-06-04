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
    version: v0.1.1
```

The base component manifest pulls the sibling board/display/LED components from
the same tag.

The repository is private, so the build environment must have GitHub access.

## Zigbee Identity

The default Basic cluster identity describes this base firmware:

- ManufacturerName: `Wilhelm Advocaat-Marum`
- ModelIdentifier: `ESP32-C6 Zigbee Base`
- DateCode: release date in `YYYYMMDD` form
- SWBuildID: SemVer without the leading `v`
- ApplicationVersion: numeric release counter
- HWVersion: reference hardware version

Products can override these defaults in two ways:

- Set the `CONFIG_ESP32_C6_ZIGBEE_BASE_*` options in project configuration.
- Assign the identity fields in `esp32_c6_zigbee_base_config_t` before calling
  `esp32_c6_zigbee_base_start()`.

Runtime config fields win over Kconfig defaults.

## Versioning And Releases

Releases use SemVer tags: `vMAJOR.MINOR.PATCH`.

For each release:

- Update `SWBuildID`, `DateCode`, and `ApplicationVersion` defaults.
- Update `components/esp32_c6_zigbee_base/idf_component.yml`.
- Update dependency examples in this README.
- Build `examples/generic_device`.
- Commit the release changes.
- Create an annotated tag, for example:

```sh
git tag -a v0.1.1 -m "Release v0.1.1"
```

Product firmware should pin this component to exact tags, not branches.

Agents working in this repository must follow `AGENTS.md`. By default,
meaningful code or documentation changes bump the patch version, build, commit,
and push. Release tags are created only when explicitly requested.
