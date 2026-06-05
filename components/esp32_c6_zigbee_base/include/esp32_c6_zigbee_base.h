#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_zigbee.h"
#include "ezbee/zcl/cluster/basic_desc.h"
#include "sdkconfig.h"

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

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_MANUFACTURER_NAME
#define CONFIG_ESP32_C6_ZIGBEE_BASE_MANUFACTURER_NAME "Wilhelm Advocaat-Marum"
#endif

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_MODEL_IDENTIFIER
#define CONFIG_ESP32_C6_ZIGBEE_BASE_MODEL_IDENTIFIER "ESP32-C6 Zigbee Base"
#endif

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE
#define CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE "20260605"
#endif

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID
#define CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID "0.1.2"
#endif

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION
#define CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION 3
#endif

#ifndef CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION
#define CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION 1
#endif

#define ESP32_C6_ZIGBEE_BASE_DEFAULT_CONFIG()                                      \
    {                                                                              \
        .manufacturer_name = CONFIG_ESP32_C6_ZIGBEE_BASE_MANUFACTURER_NAME,        \
        .model_identifier = CONFIG_ESP32_C6_ZIGBEE_BASE_MODEL_IDENTIFIER,          \
        .product_url = NULL,                                                        \
        .generic_device_type = EZB_ZCL_BASIC_GENERIC_DEVICE_TYPE_UNSPECIFIED,       \
        .date_code = CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE,                        \
        .sw_build_id = CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID,                    \
        .application_version = CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION,     \
        .hardware_version = CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION,          \
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
    const char *product_url;
    uint8_t generic_device_type;
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

typedef struct {
    const char *manufacturer_name;
    const char *model_identifier;
    const char *product_url;
    uint8_t generic_device_type;
    const char *date_code;
    const char *sw_build_id;
    uint8_t application_version;
    uint8_t hardware_version;
} esp32_c6_zigbee_base_identity_config_t;

typedef struct {
    int boot_gpio;
    int external_reset_gpio;
    uint32_t factory_reset_hold_ms;
} esp32_c6_zigbee_base_board_config_t;

typedef struct {
    uint32_t primary_channel_mask;
    uint32_t secondary_channel_mask;
    const char *storage_partition_name;
    const char *app_nvs_namespace;
} esp32_c6_zigbee_base_zigbee_config_t;

typedef struct {
    bool enabled;
    uint8_t endpoint_id;
} esp32_c6_zigbee_base_endpoint_feature_config_t;

typedef struct {
    esp32_c6_zigbee_base_identity_config_t identity;
    esp32_c6_zigbee_base_board_config_t board;
    esp32_c6_zigbee_base_zigbee_config_t zigbee;
    esp32_c6_zigbee_base_endpoint_feature_config_t outlet;
    esp32_c6_zigbee_base_endpoint_feature_config_t display;
    esp32_c6_zigbee_base_endpoint_feature_config_t status_led;
    esp32_c6_zigbee_base_extra_endpoints_cb_t extra_endpoints_cb;
    esp32_c6_zigbee_base_zcl_action_cb_t zcl_action_cb;
    void *callback_ctx;
} esp32_c6_zigbee_base_product_config_t;

#define ESP32_C6_ZIGBEE_BASE_DEFAULT_PRODUCT_CONFIG()                                  \
    {                                                                                  \
        .identity = {                                                                  \
            .manufacturer_name = CONFIG_ESP32_C6_ZIGBEE_BASE_MANUFACTURER_NAME,        \
            .model_identifier = CONFIG_ESP32_C6_ZIGBEE_BASE_MODEL_IDENTIFIER,          \
            .product_url = NULL,                                                       \
            .generic_device_type = EZB_ZCL_BASIC_GENERIC_DEVICE_TYPE_UNSPECIFIED,      \
            .date_code = CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE,                        \
            .sw_build_id = CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID,                    \
            .application_version = CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION,     \
            .hardware_version = CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION,          \
        },                                                                             \
        .board = {                                                                     \
            .boot_gpio = ESP32_C6_ZIGBEE_BASE_DEFAULT_BOOT_GPIO,                       \
            .external_reset_gpio = ESP32_C6_ZIGBEE_BASE_DEFAULT_EXTERNAL_RESET_GPIO,   \
            .factory_reset_hold_ms = ESP32_C6_ZIGBEE_BASE_DEFAULT_FACTORY_RESET_HOLD_MS, \
        },                                                                             \
        .zigbee = {                                                                    \
            .primary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK, \
            .secondary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK, \
            .storage_partition_name = ESP32_C6_ZIGBEE_BASE_DEFAULT_STORAGE_PARTITION,  \
            .app_nvs_namespace = ESP32_C6_ZIGBEE_BASE_DEFAULT_NVS_NAMESPACE,           \
        },                                                                             \
        .outlet = { .enabled = true, .endpoint_id = 10 },                              \
        .display = { .enabled = true, .endpoint_id = 11 },                             \
        .status_led = { .enabled = true, .endpoint_id = 12 },                          \
        .extra_endpoints_cb = NULL,                                                    \
        .zcl_action_cb = NULL,                                                         \
        .callback_ctx = NULL,                                                          \
    }

esp_err_t esp32_c6_zigbee_base_start(const esp32_c6_zigbee_base_config_t *config);
esp_err_t esp32_c6_zigbee_base_start_product(const esp32_c6_zigbee_base_product_config_t *product_config);
bool esp32_c6_zigbee_base_is_joined(void);

#ifdef __cplusplus
}
#endif
