#include "esp32_c6_zigbee_base.h"

/*
 * Generic device product config.
 *
 * Things to change for a real product:
 * - identity: manufacturer_name, model_identifier, product_url, generic_device_type.
 * - identity: date_code, sw_build_id, application_version, hardware_version for releases.
 * - board: reset/input pins if the board wiring differs.
 * - outlet/display/status_led: endpoint IDs and enabled flags.
 */
const esp32_c6_zigbee_base_product_config_t generic_device_config = {
    /*
     * Basic cluster identity shown by controllers.
     *
     * generic_device_type is required. This example is a plug-in outlet style
     * device, so it reports PlugInUnit instead of a bridge/controller type.
     */
    .identity = {
        .manufacturer_name = "Wilhelm Advocaat-Marum",
        .model_identifier = "ESP32-C6 Zigbee Base",
        .product_url = "https://github.com/wilhel1812/esp32-c6-zigbee-base",
        .generic_device_type = EZB_ZCL_BASIC_GENERIC_DEVICE_TYPE_PLUG_IN_UNIT,
        .date_code = CONFIG_ESP32_C6_ZIGBEE_BASE_DATE_CODE,
        .sw_build_id = CONFIG_ESP32_C6_ZIGBEE_BASE_SW_BUILD_ID,
        .application_version = CONFIG_ESP32_C6_ZIGBEE_BASE_APPLICATION_VERSION,
        .hardware_version = CONFIG_ESP32_C6_ZIGBEE_BASE_HARDWARE_VERSION,
    },

    /*
     * Board-local inputs.
     *
     * boot_gpio is the Waveshare ESP32-C6-LCD-1.3 BOOT button. Use -1 for
     * external_reset_gpio when no second reset button is fitted.
     */
    .board = {
        .boot_gpio = 9,
        .external_reset_gpio = -1,
        .factory_reset_hold_ms = 8000,
    },

    /*
     * Zigbee runtime defaults.
     *
     * primary_channel_mask uses the standard Zigbee 2.4 GHz channel range. The
     * secondary mask is 0 because no fallback channel set is currently needed.
     */
    .zigbee = {
        .primary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_PRIMARY_CHANNEL_MASK,
        .secondary_channel_mask = ESP32_C6_ZIGBEE_BASE_DEFAULT_SECONDARY_CHANNEL_MASK,
        .storage_partition_name = ESP32_C6_ZIGBEE_BASE_DEFAULT_STORAGE_PARTITION,
        .app_nvs_namespace = ESP32_C6_ZIGBEE_BASE_DEFAULT_NVS_NAMESPACE,
    },

    /*
     * Product features.
     *
     * Endpoint IDs must stay unique. Set enabled=false to omit a feature
     * endpoint from this firmware image.
     */
    .outlet = {
        .enabled = true,
        .endpoint_id = 10,
    },
    .display = {
        .enabled = true,
        .endpoint_id = 11,
    },
    .status_led = {
        .enabled = true,
        .endpoint_id = 12,
    },

    .extra_endpoints_cb = NULL,
    .zcl_action_cb = NULL,
    .callback_ctx = NULL,
};
