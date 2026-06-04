#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_zigbee.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK   (0x07FFF800U)
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK (0U)
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_STORAGE_PARTITION      "zb_storage"
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_NVS_NAMESPACE          "zigbee_base"
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_BOOT_GPIO              (9)
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_EXTERNAL_RESET_GPIO    (-1)
#define ESP32_C6_ZIGBEE_BASE_DEFAULT_FACTORY_RESET_HOLD_MS  (8000)

#define ESP32_C6_ZIGBEE_BASE_DEFAULT_CONFIG()                                      \
    {                                                                              \
        .manufacturer_name = "esp32-c6-zigbee-base",                               \
        .model_identifier = "Generic Zigbee Base",                                 \
        .date_code = "20260604",                                                   \
        .sw_build_id = "0.1.0",                                                    \
        .application_version = 1,                                                   \
        .hardware_version = 1,                                                      \
        .primary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK,  \
        .secondary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK, \
        .storage_partition_name = ESP32_C6_ZIGBEE_BASE_DEFAULT_STORAGE_PARTITION,   \
        .app_nvs_namespace = ESP32_C6_ZIGBEE_BASE_DEFAULT_NVS_NAMESPACE,            \
        .boot_gpio = ESP32_C6_ZIGBEE_BASE_DEFAULT_BOOT_GPIO,                        \
        .external_reset_gpio = ESP32_C6_ZIGBEE_BASE_DEFAULT_EXTERNAL_RESET_GPIO,    \
        .factory_reset_hold_ms = ESP32_C6_ZIGBEE_BASE_DEFAULT_FACTORY_RESET_HOLD_MS, \
        .onoff_endpoint_id = 10,                                                    \
        .display_light_endpoint_id = 11,                                            \
        .status_led_endpoint_id = 12,                                               \
        .extra_endpoints_cb = NULL,                                                 \
        .zcl_action_cb = NULL,                                                      \
        .callback_ctx = NULL,                                                       \
    }

typedef esp_err_t (*esp32_c6_zigbee_base_extra_endpoints_cb_t)(ezb_af_device_desc_t dev_desc, void *ctx);
typedef bool (*esp32_c6_zigbee_base_zcl_action_cb_t)(ezb_zcl_core_action_callback_id_t callback_id,
                                                     void *message,
                                                     void *ctx);

typedef struct {
    const char *manufacturer_name;
    const char *model_identifier;
    const char *date_code;
    const char *sw_build_id;
    uint8_t application_version;
    uint8_t hardware_version;

    uint32_t primary_channel_mask;
    uint32_t secondary_channel_mask;
    const char *storage_partition_name;
    const char *app_nvs_namespace;

    int boot_gpio;
    int external_reset_gpio;
    uint32_t factory_reset_hold_ms;

    uint8_t onoff_endpoint_id;
    uint8_t display_light_endpoint_id;
    uint8_t status_led_endpoint_id;

    esp32_c6_zigbee_base_extra_endpoints_cb_t extra_endpoints_cb;
    esp32_c6_zigbee_base_zcl_action_cb_t zcl_action_cb;
    void *callback_ctx;
} esp32_c6_zigbee_base_config_t;

esp_err_t esp32_c6_zigbee_base_start(const esp32_c6_zigbee_base_config_t *config);
bool esp32_c6_zigbee_base_is_joined(void);

#ifdef __cplusplus
}
#endif
