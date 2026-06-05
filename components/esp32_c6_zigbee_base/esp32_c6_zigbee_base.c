#include "esp32_c6_zigbee_base.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board_input.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ezbee/nwk.h"
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/basic.h"
#include "ezbee/zcl/cluster/identify.h"
#include "ezbee/zcl/cluster/level_desc.h"
#include "ezbee/zcl/zcl_general_cmd.h"

#include "status_display.h"
#include "status_led.h"

#define ZCL_STRING_MAX_LEN 32
#define ZCL_PRODUCT_URL_MAX_LEN 64
#define LINK_QUALITY_REFRESH_MS 5000

static const char *TAG = "ZB_BASE";

static esp32_c6_zigbee_base_config_t s_config;
static bool s_joined;
static bool s_output_on;
static bool s_display_light_on = true;
static uint8_t s_display_light_level = 0xff;
static bool s_status_led_light_on = true;
static uint8_t s_status_led_light_level = 0xff;
static uint32_t s_identify_generation;
static int64_t s_identify_end_us;
static uint8_t s_manufacturer_zcl[ZCL_STRING_MAX_LEN + 1];
static uint8_t s_model_zcl[ZCL_STRING_MAX_LEN + 1];
static uint8_t s_product_url_zcl[ZCL_PRODUCT_URL_MAX_LEN + 1];
static uint8_t s_date_code_zcl[ZCL_STRING_MAX_LEN + 1];
static uint8_t s_sw_build_zcl[ZCL_STRING_MAX_LEN + 1];
static TaskHandle_t s_link_quality_task;

static void request_link_quality_refresh(void)
{
    if (s_link_quality_task) {
        xTaskNotifyGive(s_link_quality_task);
    }
}

static void zcl_string_set(uint8_t *dest, size_t max_len, const char *value)
{
    size_t len = value ? strnlen(value, max_len) : 0;
    dest[0] = len;
    if (len > 0) {
        memcpy(&dest[1], value, len);
    }
}

static void copy_config(const esp32_c6_zigbee_base_config_t *config)
{
    esp32_c6_zigbee_base_config_t defaults = ESP32_C6_ZIGBEE_BASE_DEFAULT_CONFIG();
    s_config = config ? *config : defaults;

    if (!s_config.manufacturer_name) {
        s_config.manufacturer_name = defaults.manufacturer_name;
    }
    if (!s_config.model_identifier) {
        s_config.model_identifier = defaults.model_identifier;
    }
    if (!s_config.date_code) {
        s_config.date_code = defaults.date_code;
    }
    if (!s_config.sw_build_id) {
        s_config.sw_build_id = defaults.sw_build_id;
    }
    if (!s_config.storage_partition_name) {
        s_config.storage_partition_name = defaults.storage_partition_name;
    }
    if (!s_config.app_nvs_namespace) {
        s_config.app_nvs_namespace = defaults.app_nvs_namespace;
    }
    if (s_config.primary_channel_mask == 0) {
        s_config.primary_channel_mask = defaults.primary_channel_mask;
    }
    if (s_config.factory_reset_hold_ms == 0) {
        s_config.factory_reset_hold_ms = defaults.factory_reset_hold_ms;
    }

    zcl_string_set(s_manufacturer_zcl, ZCL_STRING_MAX_LEN, s_config.manufacturer_name);
    zcl_string_set(s_model_zcl, ZCL_STRING_MAX_LEN, s_config.model_identifier);
    zcl_string_set(s_product_url_zcl, ZCL_PRODUCT_URL_MAX_LEN, s_config.product_url);
    zcl_string_set(s_date_code_zcl, ZCL_STRING_MAX_LEN, s_config.date_code);
    zcl_string_set(s_sw_build_zcl, ZCL_STRING_MAX_LEN, s_config.sw_build_id);
}

static void update_visual_state(status_led_mode_t led_mode, status_display_state_t display_state)
{
    status_led_set_mode(led_mode);
    status_display_set_state(display_state);
    switch (display_state) {
    case STATUS_DISPLAY_PAIRING:
        status_display_set_network_status(STATUS_DISPLAY_NETWORK_PAIRING, "PAIRING");
        break;
    case STATUS_DISPLAY_REJOINING:
        status_display_set_network_status(STATUS_DISPLAY_NETWORK_REJOINING, "REJOINING");
        break;
    case STATUS_DISPLAY_ERROR:
        status_display_set_network_status(STATUS_DISPLAY_NETWORK_OFFLINE, "ERROR");
        break;
    case STATUS_DISPLAY_IDENTIFYING:
        status_display_set_network_status(s_joined ? STATUS_DISPLAY_NETWORK_JOINED : STATUS_DISPLAY_NETWORK_OFFLINE, "IDENTIFY");
        break;
    case STATUS_DISPLAY_RESET_HOLD:
        status_display_set_network_status(s_joined ? STATUS_DISPLAY_NETWORK_JOINED : STATUS_DISPLAY_NETWORK_OFFLINE, "RESET");
        break;
    case STATUS_DISPLAY_FACTORY_RESETTING:
        status_display_set_network_status(STATUS_DISPLAY_NETWORK_OFFLINE, "RESETTING");
        break;
    default:
        status_display_set_network_status(s_joined ? STATUS_DISPLAY_NETWORK_JOINED : STATUS_DISPLAY_NETWORK_OFFLINE,
                                          s_joined ? "JOINED" : "OFFLINE");
        break;
    }
    if (!s_joined) {
        status_display_set_link_quality(false, 0, 0);
    }
}

static void set_joined_visual_state(void)
{
    status_led_set_joined(s_output_on);
    status_display_set_network_status(STATUS_DISPLAY_NETWORK_JOINED, "JOINED");
    status_display_set_output_state(s_output_on);
    request_link_quality_refresh();
}

static esp_err_t load_u8_key(nvs_handle_t handle, const char *key, uint8_t *value)
{
    esp_err_t err = nvs_get_u8(handle, key, value);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

static esp_err_t load_app_state(void)
{
    nvs_handle_t handle;
    uint8_t on = 0;
    uint8_t level = 0;
    esp_err_t ret = nvs_open(s_config.app_nvs_namespace, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        s_output_on = false;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to open app NVS");

    ret = load_u8_key(handle, "onoff", &on);
    ESP_GOTO_ON_ERROR(ret, done, TAG, "failed to read output state");
    s_output_on = on != 0;

    on = s_display_light_on ? 1 : 0;
    level = s_display_light_level;
    ESP_GOTO_ON_ERROR(load_u8_key(handle, "disp_on", &on), done, TAG, "failed to read display on state");
    ESP_GOTO_ON_ERROR(load_u8_key(handle, "disp_level", &level), done, TAG, "failed to read display level");
    s_display_light_on = on != 0;
    s_display_light_level = level;

    on = s_status_led_light_on ? 1 : 0;
    level = s_status_led_light_level;
    ESP_GOTO_ON_ERROR(load_u8_key(handle, "led_on", &on), done, TAG, "failed to read LED on state");
    ESP_GOTO_ON_ERROR(load_u8_key(handle, "led_level", &level), done, TAG, "failed to read LED level");
    s_status_led_light_on = on != 0;
    s_status_led_light_level = level;

done:
    nvs_close(handle);
    return ret;
}

static esp_err_t save_app_state(void)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(s_config.app_nvs_namespace, NVS_READWRITE, &handle), TAG, "failed to open app NVS");
    esp_err_t err = nvs_set_u8(handle, "onoff", s_output_on ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "disp_on", s_display_light_on ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "disp_level", s_display_light_level);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "led_on", s_status_led_light_on ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "led_level", s_status_led_light_level);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void apply_user_lights(void)
{
    ESP_LOGI(TAG, "Display backlight on=%u level=%u", s_display_light_on, s_display_light_level);
    status_display_set_backlight(s_display_light_on ? s_display_light_level : 0);
    status_led_set_user_light(s_status_led_light_on, s_status_led_light_level);
}

static void report_onoff_state(void)
{
    ezb_zcl_report_attr_cmd_t report = {
        .cmd_ctrl = {
            .dst_addr = EZB_ADDRESS_SHORT(0x0000),
            .dst_ep = 1,
            .src_ep = s_config.onoff_endpoint_id,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
            .manuf_code = EZB_ZCL_STD_MANUF_CODE,
            .fc = {
                .direction = 1,
                .dis_default_rsp = 1,
            },
        },
        .payload = {
            .attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        },
    };

    ezb_err_t err = ezb_zcl_report_attr_cmd_req(&report);
    if (err != EZB_ERR_NONE) {
        ESP_LOGD(TAG, "OnOff report request skipped/failed: 0x%04x", err);
    }
}

static void set_output_state(bool on, bool flash_command)
{
    s_output_on = on;
    ESP_ERROR_CHECK_WITHOUT_ABORT(save_app_state());

    if (flash_command) {
        status_led_flash_command(on);
    } else {
        status_led_set_joined(on);
    }
    status_display_set_network_status(STATUS_DISPLAY_NETWORK_JOINED, "JOINED");
    status_display_set_output_state(on);
    request_link_quality_refresh();
    if (flash_command) {
        status_display_show_event(STATUS_DISPLAY_ICON_POWER, on ? "OUTLET ON" : "OUTLET OFF", "ZIGBEE COMMAND", 3000);
    }
    report_onoff_state();
}

static void restore_zcl_onoff_attr(void)
{
    /*
     * The OnOff attribute is initialized from NVS before endpoint registration.
     * After the Zigbee stack is active, this SDK treats the descriptor as owned
     * by the stack and rejects direct descriptor writes.
     */
}

static void identify_timeout_task(void *arg)
{
    uint32_t generation = (uint32_t)(uintptr_t)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(250));
        if (generation != s_identify_generation) {
            break;
        }
        if (esp_timer_get_time() >= s_identify_end_us) {
            if (s_joined) {
                set_joined_visual_state();
            } else {
                update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
            }
            break;
        }
    }

    vTaskDelete(NULL);
}

static void start_identify_visual(uint16_t identify_time_s)
{
    if (identify_time_s == 0) {
        s_identify_generation++;
        if (s_joined) {
            set_joined_visual_state();
        }
        return;
    }

    s_identify_generation++;
    s_identify_end_us = esp_timer_get_time() + (int64_t)identify_time_s * 1000000;
    update_visual_state(STATUS_LED_MODE_IDENTIFYING, STATUS_DISPLAY_IDENTIFYING);
    xTaskCreate(identify_timeout_task, "identify_timeout", 3072, (void *)(uintptr_t)s_identify_generation, 4, NULL);
}

static void zigbee_factory_reset_cb(void *ctx)
{
    update_visual_state(STATUS_LED_MODE_FACTORY_RESET, STATUS_DISPLAY_FACTORY_RESETTING);
    if (s_joined) {
        ezb_bdb_reset_via_local_action();
    } else {
        esp_zigbee_factory_reset();
    }
}

static uint8_t level_percent(uint8_t level)
{
    return (uint8_t)((uint32_t)level * 100 / 0xff);
}

static void show_level_event(status_display_icon_t icon, const char *name, bool on, uint8_t level)
{
    char title[32];
    if (!on) {
        snprintf(title, sizeof(title), "%s OFF", name);
    } else {
        snprintf(title, sizeof(title), "%s %u%%", name, level_percent(level));
    }
    status_display_show_event(icon, title, "ZIGBEE COMMAND", 3000);
}

static void board_input_handler(board_input_event_t event, uint8_t progress, void *ctx)
{
    switch (event) {
    case BOARD_INPUT_EVENT_RESET_HOLD_START:
        update_visual_state(STATUS_LED_MODE_RESET_HOLD, STATUS_DISPLAY_RESET_HOLD);
        status_display_set_reset_progress(progress);
        break;
    case BOARD_INPUT_EVENT_RESET_HOLD_PROGRESS:
        status_display_set_reset_progress(progress);
        break;
    case BOARD_INPUT_EVENT_RESET_HOLD_CANCELLED:
        if (s_joined) {
            set_joined_visual_state();
        } else {
            update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
        }
        break;
    case BOARD_INPUT_EVENT_FACTORY_RESET_REQUEST:
        ESP_LOGW(TAG, "Local factory reset requested");
        status_display_set_reset_progress(100);
        update_visual_state(STATUS_LED_MODE_FACTORY_RESET, STATUS_DISPLAY_FACTORY_RESETTING);
        if (esp_zigbee_is_started()) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_zigbee_task_queue_post(zigbee_factory_reset_cb, NULL));
        }
        break;
    default:
        break;
    }
}

static void schedule_network_steering(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS((uint32_t)(uintptr_t)arg));

    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
    esp_zigbee_lock_release();

    vTaskDelete(NULL);
}

static void retry_network_steering(uint32_t delay_ms)
{
    xTaskCreate(schedule_network_steering, "zb_steer_retry", 3072, (void *)delay_ms, 5, NULL);
}

static bool read_best_link_quality(uint8_t *lqi, int8_t *rssi)
{
    ezb_nwk_info_iterator_t iterator = EZB_NWK_INFO_ITERATOR_INIT;
    ezb_nwk_neighbor_info_t info = {0};
    bool found = false;
    uint8_t best_lqi = 0;
    int8_t best_rssi = 0;

    esp_zigbee_lock_acquire(portMAX_DELAY);
    while (ezb_nwk_get_next_neighbor(&iterator, &info) == EZB_ERR_NONE) {
        bool is_router = info.device_type == EZB_NWK_DEVICE_TYPE_COORDINATOR ||
                         info.device_type == EZB_NWK_DEVICE_TYPE_ROUTER;
        if (is_router && info.lqi > best_lqi) {
            found = true;
            best_lqi = info.lqi;
            best_rssi = info.rssi;
        }
    }
    esp_zigbee_lock_release();

    if (found) {
        *lqi = best_lqi;
        *rssi = best_rssi;
    }
    return found;
}

static void link_quality_task(void *arg)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(LINK_QUALITY_REFRESH_MS));

        if (!s_joined) {
            status_display_set_link_quality(false, 0, 0);
            continue;
        }

        uint8_t lqi = 0;
        int8_t rssi = 0;
        bool known = read_best_link_quality(&lqi, &rssi);
        status_display_set_link_quality(known, lqi, rssi);
        if (known) {
            ESP_LOGD(TAG, "Zigbee link quality: LQI %u RSSI %d", lqi, rssi);
        } else {
            ESP_LOGD(TAG, "Zigbee link quality unknown");
        }
    }
}

static bool zigbee_signal_handler(const ezb_app_signal_t *app_signal)
{
    ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);

    switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
        break;

    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Device started in%s factory-reset mode", ezb_bdb_is_factory_new() ? "" : " non");
            if (ezb_bdb_is_factory_new()) {
                s_joined = false;
                update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
                ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
            } else {
                s_joined = true;
                restore_zcl_onoff_attr();
                set_joined_visual_state();
                ESP_LOGI(TAG, "Device rebooted on joined network");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status 0x%02x", ezb_app_signal_to_string(signal_type), status);
            status_led_error_blink();
            status_display_set_state(STATUS_DISPLAY_ERROR);
            retry_network_steering(1000);
        }
    } break;

    case EZB_BDB_SIGNAL_STEERING: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Network steering completed: PAN 0x%04hx channel %u short 0x%04hx",
                     ezb_nwk_get_panid(), ezb_nwk_get_current_channel(), ezb_nwk_get_short_address());
            s_joined = true;
            restore_zcl_onoff_attr();
            set_joined_visual_state();
        } else {
            ESP_LOGW(TAG, "Network steering failed with status 0x%02x", status);
            s_joined = false;
            status_led_error_blink();
            status_display_set_state(STATUS_DISPLAY_ERROR);
            update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
            retry_network_steering(3000);
        }
    } break;

    case EZB_ZDO_SIGNAL_LEAVE: {
        ezb_zdo_signal_leave_params_t *params = (ezb_zdo_signal_leave_params_t *)ezb_app_signal_get_params(app_signal);
        ESP_LOGW(TAG, "Leave signal type %u", params ? params->leave_type : 0xff);
        s_joined = false;
        if (params && params->leave_type == EZB_ZDO_LEAVE_TYPE_REJOIN) {
            update_visual_state(STATUS_LED_MODE_REJOINING, STATUS_DISPLAY_REJOINING);
        } else {
            update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
            esp_restart();
        }
    } break;

    case EZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
    case EZB_NWK_SIGNAL_NETWORK_STATUS:
        ESP_LOGW(TAG, "Network health signal: %s", ezb_app_signal_to_string(signal_type));
        update_visual_state(STATUS_LED_MODE_REJOINING, STATUS_DISPLAY_REJOINING);
        break;

    case EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: {
        uint8_t duration = *(uint8_t *)ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "Permit join is %s for network 0x%04hx", duration ? "open" : "closed", ezb_nwk_get_panid());
    } break;

    default:
        ESP_LOGI(TAG, "Zigbee signal: %s (0x%02x)", ezb_app_signal_to_string(signal_type), signal_type);
        break;
    }

    return true;
}

static void handle_set_attr_value(ezb_zcl_set_attr_value_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, , TAG, "empty ZCL set-attribute message");

    ESP_LOGI(TAG, "Set attribute: ep=%d cluster=0x%04x role=%s status=0x%02x",
             message->info.dst_ep,
             message->info.cluster_id,
             message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client",
             message->info.status);

    if (message->info.cluster_role != EZB_ZCL_CLUSTER_SERVER ||
        message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        return;
    }

    if (message->info.dst_ep == s_config.onoff_endpoint_id &&
        message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
        message->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        bool on = *(uint8_t *)message->in.attribute.data.value;
        ESP_LOGI(TAG, "Set OnOff to %d", on);
        set_output_state(on, true);
        return;
    }

    if ((message->info.dst_ep == s_config.display_light_endpoint_id ||
         message->info.dst_ep == s_config.status_led_endpoint_id) &&
        message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
        message->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        bool on = *(uint8_t *)message->in.attribute.data.value;
        if (message->info.dst_ep == s_config.display_light_endpoint_id) {
            s_display_light_on = on;
        } else {
            s_status_led_light_on = on;
        }
        apply_user_lights();
        show_level_event(message->info.dst_ep == s_config.display_light_endpoint_id ? STATUS_DISPLAY_ICON_SUN : STATUS_DISPLAY_ICON_SPARKLES,
                         message->info.dst_ep == s_config.display_light_endpoint_id ? "DISPLAY" : "LED",
                         on,
                         message->info.dst_ep == s_config.display_light_endpoint_id ? s_display_light_level : s_status_led_light_level);
        ESP_ERROR_CHECK_WITHOUT_ABORT(save_app_state());
        return;
    }

    if ((message->info.dst_ep == s_config.display_light_endpoint_id ||
         message->info.dst_ep == s_config.status_led_endpoint_id) &&
        message->info.cluster_id == EZB_ZCL_CLUSTER_ID_LEVEL &&
        message->in.attribute.id == EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID) {
        uint8_t level = *(uint8_t *)message->in.attribute.data.value;
        if (message->info.dst_ep == s_config.display_light_endpoint_id) {
            s_display_light_level = level;
        } else {
            s_status_led_light_level = level;
        }
        apply_user_lights();
        show_level_event(message->info.dst_ep == s_config.display_light_endpoint_id ? STATUS_DISPLAY_ICON_SUN : STATUS_DISPLAY_ICON_SPARKLES,
                         message->info.dst_ep == s_config.display_light_endpoint_id ? "DISPLAY" : "LED",
                         message->info.dst_ep == s_config.display_light_endpoint_id ? s_display_light_on : s_status_led_light_on,
                         level);
        ESP_ERROR_CHECK_WITHOUT_ABORT(save_app_state());
        return;
    }

    if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_IDENTIFY &&
        message->in.attribute.id == EZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID) {
        uint16_t identify_time = *(uint16_t *)message->in.attribute.data.value;
        ESP_LOGI(TAG, "IdentifyTime set to %u seconds", identify_time);
        start_identify_visual(identify_time);
        return;
    }
}

static void handle_identify_effect(ezb_zcl_identify_effect_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, , TAG, "empty identify effect message");
    if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        return;
    }

    ESP_LOGI(TAG, "Identify effect id=%u variant=%u", message->in.effect_id, message->in.effect_variant);
    message->out.result = EZB_ZCL_STATUS_SUCCESS;
    start_identify_visual(5);
}

static void handle_basic_factory_reset(ezb_zcl_basic_reset_factory_default_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, , TAG, "empty Basic reset message");
    if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        return;
    }

    ESP_LOGW(TAG, "Basic ResetToFactoryDefaults received");
    message->out.result = EZB_ZCL_STATUS_SUCCESS;
    zigbee_factory_reset_cb(NULL);
}

static esp_err_t basic_attr_add_or_set(ezb_zcl_cluster_desc_t basic_desc, uint16_t attr_id, const void *value)
{
    ezb_zcl_attr_desc_t attr = ezb_zcl_cluster_get_attr_desc(basic_desc, attr_id, EZB_ZCL_STD_MANUF_CODE);
    if (attr) {
        return esp_zigbee_err_to_esp(ezb_zcl_attr_desc_set_value(attr, value));
    }

    return esp_zigbee_err_to_esp(ezb_zcl_basic_cluster_desc_add_attr(basic_desc, attr_id, value));
}

static esp_err_t populate_basic_attrs(ezb_af_ep_desc_t ep_desc)
{
    ezb_zcl_cluster_desc_t basic_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
    static uint8_t power_source = EZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    static uint8_t device_enabled = 1;

    ESP_RETURN_ON_FALSE(basic_desc, ESP_ERR_NOT_FOUND, TAG, "basic cluster missing");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                              s_manufacturer_zcl), TAG, "manufacturer attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                              s_model_zcl), TAG, "model attr failed");
    if (s_config.product_url && s_product_url_zcl[0] > 0) {
        ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID,
                                                  s_product_url_zcl), TAG, "product URL attr failed");
    }
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
                                              &s_config.application_version), TAG, "application version attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_HW_VERSION_ID,
                                              &s_config.hardware_version), TAG, "hardware version attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_DATE_CODE_ID,
                                              s_date_code_zcl), TAG, "date code attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source),
                        TAG, "power source attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID, &s_config.generic_device_type),
                        TAG, "generic device type attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID, &device_enabled),
                        TAG, "device enabled attr failed");
    ESP_RETURN_ON_ERROR(basic_attr_add_or_set(basic_desc, EZB_ZCL_ATTR_BASIC_SW_BUILD_ID_ID,
                                              s_sw_build_zcl), TAG, "software build attr failed");

    return ESP_OK;
}

static esp_err_t set_cluster_attr(ezb_af_ep_desc_t ep_desc, uint16_t cluster_id, uint16_t attr_id, const void *value)
{
    ezb_zcl_cluster_desc_t cluster = ezb_af_endpoint_get_cluster_desc(ep_desc, cluster_id, EZB_ZCL_CLUSTER_SERVER);
    ESP_RETURN_ON_FALSE(cluster, ESP_ERR_NOT_FOUND, TAG, "cluster 0x%04x missing", cluster_id);

    ezb_zcl_attr_desc_t attr = ezb_zcl_cluster_get_attr_desc(cluster, attr_id, EZB_ZCL_STD_MANUF_CODE);
    ESP_RETURN_ON_FALSE(attr, ESP_ERR_NOT_FOUND, TAG, "attribute 0x%04x missing", attr_id);
    return esp_zigbee_err_to_esp(ezb_zcl_attr_desc_set_value(attr, value));
}

static void zcl_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        handle_set_attr_value(message);
        break;

    case EZB_ZCL_CORE_IDENTIFY_EFFECT_CB_ID:
        handle_identify_effect(message);
        break;

    case EZB_ZCL_CORE_BASIC_RESET_TO_FACTORY_DEFAULT_CB_ID:
        handle_basic_factory_reset(message);
        break;

    case EZB_ZCL_CORE_CONFIG_REPORT_RSP_CB_ID:
        ESP_LOGI(TAG, "Configure reporting response received");
        break;

    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: {
        ezb_zcl_cmd_default_rsp_message_t *default_rsp = (ezb_zcl_cmd_default_rsp_message_t *)message;
        ESP_LOGI(TAG, "Default response status 0x%02x", default_rsp->in.status_code);
    } break;

    default:
        if (s_config.zcl_action_cb && s_config.zcl_action_cb(callback_id, message, s_config.callback_ctx)) {
            break;
        }
        ESP_LOGW(TAG, "Unhandled ZCL action callback 0x%04lx", callback_id);
        break;
    }
}

static esp_err_t create_endpoints(void)
{
    ezb_af_device_desc_t dev_desc = ezb_af_create_device_desc();
    ezb_zha_mains_power_outlet_config_t outlet_cfg = EZB_ZHA_MAINS_POWER_OUTLET_CONFIG();
    ezb_zha_dimmable_light_config_t display_cfg = EZB_ZHA_DIMMABLE_LIGHT_CONFIG();
    ezb_zha_dimmable_light_config_t led_cfg = EZB_ZHA_DIMMABLE_LIGHT_CONFIG();
    ezb_af_ep_desc_t outlet_ep = NULL;
    ezb_af_ep_desc_t display_ep = NULL;
    ezb_af_ep_desc_t led_ep = NULL;
    ezb_zcl_cluster_desc_t onoff_desc = {0};
    ezb_zcl_attr_desc_t onoff_attr = NULL;
    uint8_t display_on = s_display_light_on ? 1 : 0;
    uint8_t led_on = s_status_led_light_on ? 1 : 0;

    ESP_RETURN_ON_FALSE(dev_desc, ESP_ERR_NO_MEM, TAG, "failed to create device descriptor");

    if (s_config.onoff_endpoint_id) {
        outlet_ep = ezb_zha_create_mains_power_outlet(s_config.onoff_endpoint_id, &outlet_cfg);
        ESP_RETURN_ON_FALSE(outlet_ep, ESP_ERR_NO_MEM, TAG, "failed to create outlet endpoint descriptor");
        ESP_RETURN_ON_ERROR(populate_basic_attrs(outlet_ep), TAG, "outlet Basic attrs failed");

        onoff_desc = ezb_af_endpoint_get_cluster_desc(outlet_ep, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER);
        ESP_RETURN_ON_FALSE(onoff_desc, ESP_ERR_NOT_FOUND, TAG, "OnOff cluster missing");

        onoff_attr = ezb_zcl_cluster_get_attr_desc(onoff_desc, EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE);
        if (onoff_attr) {
            ezb_zcl_attr_access_t access = ezb_zcl_attr_desc_get_access(onoff_attr);
            esp_err_t access_err = esp_zigbee_err_to_esp(ezb_zcl_attr_desc_set_access(onoff_attr,
                                                                                      access | EZB_ZCL_ATTR_ACCESS_REPORTING));
            if (access_err != ESP_OK) {
                ESP_LOGW(TAG, "OnOff reporting access flag not changed: %s", esp_err_to_name(access_err));
            }
            ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_zcl_attr_desc_set_value(onoff_attr, &s_output_on)));
        }
        ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, outlet_ep));
    }

    if (s_config.display_light_endpoint_id) {
        display_cfg.on_off_cfg.on_off = display_on;
        display_cfg.level_cfg.current_level = s_display_light_level;
        display_ep = ezb_zha_create_dimmable_light(s_config.display_light_endpoint_id, &display_cfg);
        ESP_RETURN_ON_FALSE(display_ep, ESP_ERR_NO_MEM, TAG, "failed to create display light endpoint descriptor");
        ESP_RETURN_ON_ERROR(populate_basic_attrs(display_ep), TAG, "display Basic attrs failed");
        ESP_RETURN_ON_ERROR(set_cluster_attr(display_ep, EZB_ZCL_CLUSTER_ID_ON_OFF,
                                             EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &display_on),
                            TAG, "display OnOff attr failed");
        ESP_RETURN_ON_ERROR(set_cluster_attr(display_ep, EZB_ZCL_CLUSTER_ID_LEVEL,
                                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, &s_display_light_level),
                            TAG, "display level attr failed");
        ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, display_ep));
    }

    if (s_config.status_led_endpoint_id) {
        led_cfg.on_off_cfg.on_off = led_on;
        led_cfg.level_cfg.current_level = s_status_led_light_level;
        led_ep = ezb_zha_create_dimmable_light(s_config.status_led_endpoint_id, &led_cfg);
        ESP_RETURN_ON_FALSE(led_ep, ESP_ERR_NO_MEM, TAG, "failed to create status LED endpoint descriptor");
        ESP_RETURN_ON_ERROR(populate_basic_attrs(led_ep), TAG, "LED Basic attrs failed");
        ESP_RETURN_ON_ERROR(set_cluster_attr(led_ep, EZB_ZCL_CLUSTER_ID_ON_OFF,
                                             EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &led_on),
                            TAG, "LED OnOff attr failed");
        ESP_RETURN_ON_ERROR(set_cluster_attr(led_ep, EZB_ZCL_CLUSTER_ID_LEVEL,
                                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, &s_status_led_light_level),
                            TAG, "LED level attr failed");
        ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, led_ep));
    }

    if (s_config.extra_endpoints_cb) {
        ESP_RETURN_ON_ERROR(s_config.extra_endpoints_cb(dev_desc, s_config.callback_ctx), TAG, "extra endpoint registration failed");
    }

    ESP_ERROR_CHECK(ezb_af_device_desc_register(dev_desc));
    ezb_zcl_core_action_handler_register(zcl_action_handler);

    return ESP_OK;
}

static esp_err_t setup_commissioning(void)
{
    ezb_aps_secur_enable_distributed_security(false);
    ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(s_config.primary_channel_mask));
    ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(s_config.secondary_channel_mask));
    ESP_ERROR_CHECK(ezb_app_signal_add_handler(zigbee_signal_handler));

    return ESP_OK;
}

static void zigbee_main_task(void *pvParameters)
{
    esp_zigbee_config_t config = {
        .device_config = {
            .device_type = EZB_NWK_DEVICE_TYPE_ROUTER,
            .install_code_policy = false,
            .zczr_config = {
                .max_children = 10,
            },
        },
        .platform_config = {
            .storage_partition_name = s_config.storage_partition_name,
#if CONFIG_SOC_IEEE802154_SUPPORTED
            .radio_config = {
                .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,
            },
#endif
        },
    };

    ESP_ERROR_CHECK(esp_zigbee_init(&config));
    ESP_ERROR_CHECK(setup_commissioning());
    ESP_ERROR_CHECK(create_endpoints());
    ESP_ERROR_CHECK(esp_zigbee_start(false));

    esp_zigbee_launch_mainloop();
    esp_zigbee_deinit();
    vTaskDelete(NULL);
}

esp_err_t esp32_c6_zigbee_base_start(const esp32_c6_zigbee_base_config_t *config)
{
    copy_config(config);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "failed to erase default NVS");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to initialize default NVS");
    ESP_RETURN_ON_ERROR(nvs_flash_init_partition(s_config.storage_partition_name), TAG, "failed to initialize Zigbee NVS partition");

    ESP_RETURN_ON_ERROR(status_led_init(), TAG, "failed to initialize status LED");
    ESP_RETURN_ON_ERROR(status_display_init(), TAG, "failed to initialize status display");
    status_display_set_state(STATUS_DISPLAY_BOOTING);
    status_led_startup_pulse();
    update_visual_state(STATUS_LED_MODE_JOINING, STATUS_DISPLAY_PAIRING);
    ESP_RETURN_ON_ERROR(load_app_state(), TAG, "failed to load app state");
    apply_user_lights();
    ESP_RETURN_ON_ERROR(board_input_init(&(board_input_config_t){
                            .boot_gpio = s_config.boot_gpio,
                            .external_reset_gpio = s_config.external_reset_gpio,
                            .factory_reset_hold_ms = s_config.factory_reset_hold_ms,
                            .callback = board_input_handler,
                        }),
                        TAG, "failed to initialize board input");

    ESP_LOGI(TAG, "Start ESP32-C6 Zigbee base");
    xTaskCreate(link_quality_task, "zb_link_quality", 3072, NULL, 4, &s_link_quality_task);
    xTaskCreate(zigbee_main_task, "Zigbee_main", 4096, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t esp32_c6_zigbee_base_start_product(const esp32_c6_zigbee_base_product_config_t *product_config)
{
    ESP_RETURN_ON_FALSE(product_config, ESP_ERR_INVALID_ARG, TAG, "product config is required");
    ESP_RETURN_ON_FALSE(product_config->identity.manufacturer_name, ESP_ERR_INVALID_ARG, TAG, "manufacturer name is required");
    ESP_RETURN_ON_FALSE(product_config->identity.model_identifier, ESP_ERR_INVALID_ARG, TAG, "model identifier is required");
    ESP_RETURN_ON_FALSE(product_config->identity.product_url, ESP_ERR_INVALID_ARG, TAG, "product URL is required");
    ESP_RETURN_ON_FALSE(product_config->identity.generic_device_type != EZB_ZCL_BASIC_GENERIC_DEVICE_TYPE_UNSPECIFIED,
                        ESP_ERR_INVALID_ARG, TAG, "generic device type is required");

    esp32_c6_zigbee_base_config_t config = ESP32_C6_ZIGBEE_BASE_DEFAULT_CONFIG();
    config.manufacturer_name = product_config->identity.manufacturer_name;
    config.model_identifier = product_config->identity.model_identifier;
    config.product_url = product_config->identity.product_url;
    config.generic_device_type = product_config->identity.generic_device_type;
    config.date_code = product_config->identity.date_code;
    config.sw_build_id = product_config->identity.sw_build_id;
    config.application_version = product_config->identity.application_version;
    config.hardware_version = product_config->identity.hardware_version;
    config.primary_channel_mask = product_config->zigbee.primary_channel_mask;
    config.secondary_channel_mask = product_config->zigbee.secondary_channel_mask;
    config.storage_partition_name = product_config->zigbee.storage_partition_name;
    config.app_nvs_namespace = product_config->zigbee.app_nvs_namespace;
    config.boot_gpio = product_config->board.boot_gpio;
    config.external_reset_gpio = product_config->board.external_reset_gpio;
    config.factory_reset_hold_ms = product_config->board.factory_reset_hold_ms;
    config.onoff_endpoint_id = product_config->outlet.enabled ? product_config->outlet.endpoint_id : 0;
    config.display_light_endpoint_id = product_config->display.enabled ? product_config->display.endpoint_id : 0;
    config.status_led_endpoint_id = product_config->status_led.enabled ? product_config->status_led.endpoint_id : 0;
    config.extra_endpoints_cb = product_config->extra_endpoints_cb;
    config.zcl_action_cb = product_config->zcl_action_cb;
    config.callback_ctx = product_config->callback_ctx;

    return esp32_c6_zigbee_base_start(&config);
}

bool esp32_c6_zigbee_base_is_joined(void)
{
    return s_joined;
}
