# ESP32-C6 Zigbee Base Roadmap

Legend: `[x]` done in code, `[~]` partially implemented / needs hardware validation, `[ ]` not started.

## Phase 1: Zigbee Device Complete

Goal: Make the ESP32-C6 behave like a complete, standards-aligned Zigbee router foundation before product-specific behavior is layered on top.

### Commissioning and Network Lifecycle

- [~] Factory-new pairing behavior
- [~] Local factory reset / leave-and-rejoin trigger
- [~] Coordinator-initiated leave handling
- [~] Rejoin after reboot
- [~] Rejoin after network loss
- [ ] Broad channel scan policy
- [x] Clear visual states for pairing, joined, rejoining, reset, identify, and error

### ZCL Behavior

- [~] Basic cluster required attributes
- [x] Identify cluster with visible LED/display behavior
- [~] OnOff cluster explicit On, Off, Toggle handling
- [x] OnOff state persistence policy
- [~] Attribute reporting
- [ ] Groups cluster
- [ ] Scenes cluster
- [ ] Confirm standards-compliant base endpoint/device shape

### Display and Local UI

- [x] Always-visible status bar
- [x] Pairing screen
- [x] Standby/current state screen
- [x] Temporary command/event overlay
- [x] Identify screen
- [x] Reset hold/factory reset screen
- [x] Rejoining/network recovery screen
- [x] Error/recovery screen
- [x] Exact Lucide icon asset pipeline
- [x] Real Zigbee LQI/RSSI status display
- [~] Battery/power status display API
- [ ] LCD color and transfer integrity validation

### Board Support

- [ ] Waveshare ESP32-C6-LCD-1.3 pin/profile documentation
- [x] BOOT button runtime input behavior
- [x] LCD backlight control
- [x] WS2812 status LED control
- [ ] Document local-development component override flow

### Validation

- [ ] Fresh join to coordinator
- [ ] Remove from coordinator and rejoin
- [ ] Power-cycle behavior
- [ ] Coordinator offline/online behavior
- [ ] Toggle from controller
- [ ] Identify command from controller
- [ ] Display/LED brightness control from controller
- [ ] BOOT long-press reset
- [ ] Long-running stability test

## Phase 2: Reusable Product API

Goal: Keep product firmware small by exposing stable base APIs for identity, visual state, endpoint registration, and product-supplied display events.

- [x] Review and define all Basic cluster identity attributes
- [x] Product identity configuration
- [~] Product endpoint registration hook
- [ ] Product event display API
- [ ] Visual state callback/hook policy
- [x] Versioning scheme and git release/tag policy
- [x] Consumer documentation for ESP-IDF dependency pinning
- [x] Sectioned product config template
- [x] Config reference documentation enforcement
- [x] Just the Docs source and Pages workflow
