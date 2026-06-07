# ESPHome Supported-Features Spike

This folder is an ESPHome experiment for the Waveshare ESP32-C6-LCD-1.3 board.
It is intentionally separate from the ESP-IDF firmware in this repository.

The goal is to use only ESPHome features currently supported on ESP32-C6:

- Zigbee network join/rejoin lifecycle.
- Real GPIO binary sensor exposure over Zigbee.
- Local SPI display rendering.
- Wi-Fi API and OTA for development.

Current ESPHome Zigbee support on ESP32 exposes binary sensors and sensors.
Controllable switch/number endpoints are not currently available on ESP32, so
this config does not recreate the outlet, display-light, or status LED Zigbee
endpoints from the ESP-IDF firmware.

## Build

Install ESPHome, then run:

```sh
esphome compile esphome/esp32-c6-lcd-zigbee-supported.yaml
```

Or compile with Docker from the repo root:

```sh
docker run --rm -v "$PWD/esphome":/config -w /config ghcr.io/esphome/esphome:2026.5.3 compile esp32-c6-lcd-zigbee-supported.yaml
```

Flash with the port for your board:

```sh
esphome run esphome/esp32-c6-lcd-zigbee-supported.yaml --device /dev/cu.usbmodem1101
```

Create a local `secrets.yaml` next to the YAML file or provide secrets through
your ESPHome dashboard:

```yaml
wifi_ssid: "your-wifi"
wifi_password: "your-password"
```
