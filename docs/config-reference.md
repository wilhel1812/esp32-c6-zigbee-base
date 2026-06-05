---
title: Config Reference
layout: default
nav_order: 2
---

# Config Reference

This page is generated from `docs/config_metadata.json`. Run `npm run docs:generate` after changing public product config fields.

Product configs use a sectioned C shape inspired by Klipper configs: identity first, then board, Zigbee runtime settings, and feature sections.

## esp32_c6_zigbee_base_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `legacy.manufacturer_name` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_MANUFACTURER_NAME | Any non-empty Zigbee character string. | ManufacturerName value for the Basic cluster. |
| `legacy.model_identifier` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_MODEL_IDENTIFIER | Any non-empty Zigbee character string. | ModelIdentifier value for the Basic cluster. |
| `legacy.product_url` | `string` | no | Omitted. | A product, support, or source URL. | ProductURL value for the Basic cluster. |
| `legacy.generic_device_type` | `uint8_t` | no | EZB_ZCL_BASIC_GENERIC_DEVICE_TYPE_UNSPECIFIED | Any SDK GenericDeviceType value. | Generic device type used by controllers for UI/icon selection. |
| `legacy.date_code` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE | YYYYMMDD for release builds. | DateCode value for the Basic cluster. |
| `legacy.sw_build_id` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID | SemVer without a leading v. | SWBuildID value for the Basic cluster. |
| `legacy.application_version` | `uint8_t` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION | 0-255. | Numeric Basic cluster ApplicationVersion. |
| `legacy.hardware_version` | `uint8_t` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION | 0-255. | Numeric Basic cluster HWVersion. |
| `legacy.primary_channel_mask` | `uint32_t` | no | ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK | Zigbee channel bitmask. | Primary channel set used while commissioning. |
| `legacy.secondary_channel_mask` | `uint32_t` | no | ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK | Zigbee channel bitmask. | Secondary channel set used while commissioning. |
| `legacy.storage_partition_name` | `string` | no | zb_storage | An ESP-IDF partition name. | NVS partition used by the Zigbee stack. |
| `legacy.app_nvs_namespace` | `string` | no | zigbee_base | An NVS namespace. | NVS namespace used for app-level persisted state. |
| `legacy.boot_gpio` | `int` | no | 9 | GPIO number. | Primary local reset/input button GPIO. |
| `legacy.external_reset_gpio` | `int` | no | -1 | GPIO number or -1. | Optional second local reset/input GPIO. |
| `legacy.factory_reset_hold_ms` | `uint32_t` | no | 8000 | Positive duration in milliseconds. | Hold duration before local factory reset is requested. |
| `legacy.onoff_endpoint_id` | `uint8_t` | no | 10 | 1-240, or 0 to disable. | Endpoint ID for the outlet OnOff endpoint. |
| `legacy.display_light_endpoint_id` | `uint8_t` | no | 11 | 1-240, or 0 to disable. | Endpoint ID for the display backlight endpoint. |
| `legacy.status_led_endpoint_id` | `uint8_t` | no | 12 | 1-240, or 0 to disable. | Endpoint ID for the status LED endpoint. |
| `legacy.extra_endpoints_cb` | `callback` | no | NULL | Function pointer or NULL. | Optional callback for registering additional endpoints. |
| `legacy.zcl_action_cb` | `callback` | no | NULL | Function pointer or NULL. | Optional callback for product-specific ZCL action handling. |
| `legacy.callback_ctx` | `void *` | no | NULL | Any pointer. | User context passed to product callbacks. |

## esp32_c6_zigbee_base_identity_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `identity.manufacturer_name` | `string` | yes | None in product config. | Any non-empty Zigbee character string. | ManufacturerName value for the Basic cluster. |
| `identity.model_identifier` | `string` | yes | None in product config. | Any non-empty Zigbee character string. | ModelIdentifier value for the Basic cluster. |
| `identity.product_url` | `string` | yes | None in product config. | A product, support, or source URL. | ProductURL value for the Basic cluster. |
| `identity.generic_device_type` | `uint8_t` | yes | None in product config. | Any SDK GenericDeviceType except Unspecified. | Generic device type used by controllers for UI/icon selection. |
| `identity.date_code` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE | YYYYMMDD for release builds. | DateCode value for the Basic cluster. |
| `identity.sw_build_id` | `string` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID | SemVer without a leading v. | SWBuildID value for the Basic cluster. |
| `identity.application_version` | `uint8_t` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION | 0-255. | Numeric Basic cluster ApplicationVersion. |
| `identity.hardware_version` | `uint8_t` | no | CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION | 0-255. | Numeric Basic cluster HWVersion. |

## esp32_c6_zigbee_base_board_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `board.boot_gpio` | `int` | yes | 9 in the reference template. | GPIO number. | Primary local reset/input button GPIO. |
| `board.external_reset_gpio` | `int` | no | -1 in the reference template. | GPIO number or -1. | Optional second local reset/input GPIO. |
| `board.factory_reset_hold_ms` | `uint32_t` | yes | 8000 in the reference template. | Positive duration in milliseconds. | Hold duration before local factory reset is requested. |

## esp32_c6_zigbee_base_zigbee_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `zigbee.primary_channel_mask` | `uint32_t` | no | ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK | Zigbee channel bitmask. | Primary channel set used while commissioning. |
| `zigbee.secondary_channel_mask` | `uint32_t` | no | ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK | Zigbee channel bitmask. | Secondary channel set used while commissioning. |
| `zigbee.storage_partition_name` | `string` | no | zb_storage | An ESP-IDF partition name. | NVS partition used by the Zigbee stack. |
| `zigbee.app_nvs_namespace` | `string` | no | zigbee_base | An NVS namespace. | NVS namespace used for app-level persisted state. |

## esp32_c6_zigbee_base_endpoint_feature_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `outlet.enabled / display.enabled / status_led.enabled` | `bool` | yes | true in the reference template. | true or false. | Controls whether the feature endpoint is registered. |
| `outlet.endpoint_id / display.endpoint_id / status_led.endpoint_id` | `uint8_t` | yes | 10, 11, and 12 in the reference template. | 1-240 when enabled. | Zigbee endpoint ID for the feature. |

## esp32_c6_zigbee_base_product_config_t

| Field path | Type | Required | Default | Allowed values | Description |
| --- | --- | --- | --- | --- | --- |
| `identity` | `section` | yes | None. | esp32_c6_zigbee_base_identity_config_t. | Product identity and release metadata. |
| `board` | `section` | yes | None. | esp32_c6_zigbee_base_board_config_t. | Board-local pins and reset behavior. |
| `zigbee` | `section` | no | Base Zigbee defaults. | esp32_c6_zigbee_base_zigbee_config_t. | Zigbee runtime and storage settings. |
| `outlet` | `section` | no | Enabled endpoint 10 in the reference template. | esp32_c6_zigbee_base_endpoint_feature_config_t. | Outlet OnOff feature endpoint. |
| `display` | `section` | no | Enabled endpoint 11 in the reference template. | esp32_c6_zigbee_base_endpoint_feature_config_t. | Display backlight feature endpoint. |
| `status_led` | `section` | no | Enabled endpoint 12 in the reference template. | esp32_c6_zigbee_base_endpoint_feature_config_t. | Status LED feature endpoint. |
| `extra_endpoints_cb` | `callback` | no | NULL | Function pointer or NULL. | Optional callback for registering additional endpoints. |
| `zcl_action_cb` | `callback` | no | NULL | Function pointer or NULL. | Optional callback for product-specific ZCL action handling. |
| `callback_ctx` | `void *` | no | NULL | Any pointer. | User context passed to product callbacks. |

