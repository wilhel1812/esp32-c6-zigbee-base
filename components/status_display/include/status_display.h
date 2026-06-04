#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_DISPLAY_BOOTING = 0,
    STATUS_DISPLAY_PAIRING,
    STATUS_DISPLAY_STANDBY,
    STATUS_DISPLAY_JOINED_OFF = STATUS_DISPLAY_STANDBY,
    STATUS_DISPLAY_JOINED_ON,
    STATUS_DISPLAY_IDENTIFYING,
    STATUS_DISPLAY_REJOINING,
    STATUS_DISPLAY_RESET_HOLD,
    STATUS_DISPLAY_FACTORY_RESETTING,
    STATUS_DISPLAY_ERROR,
} status_display_state_t;

typedef enum {
    STATUS_DISPLAY_ICON_RADIO_TOWER = 0,
    STATUS_DISPLAY_ICON_WIFI,
    STATUS_DISPLAY_ICON_WIFI_OFF,
    STATUS_DISPLAY_ICON_SIGNAL_HIGH,
    STATUS_DISPLAY_ICON_SIGNAL_MEDIUM,
    STATUS_DISPLAY_ICON_SIGNAL_LOW,
    STATUS_DISPLAY_ICON_SIGNAL_ZERO,
    STATUS_DISPLAY_ICON_POWER,
    STATUS_DISPLAY_ICON_SUN,
    STATUS_DISPLAY_ICON_SPARKLES,
    STATUS_DISPLAY_ICON_SEND,
    STATUS_DISPLAY_ICON_CHECK,
    STATUS_DISPLAY_ICON_LOCATE_FIXED,
    STATUS_DISPLAY_ICON_REFRESH_CW,
    STATUS_DISPLAY_ICON_ROTATE_CCW,
    STATUS_DISPLAY_ICON_TRIANGLE_ALERT,
    STATUS_DISPLAY_ICON_BATTERY,
    STATUS_DISPLAY_ICON_BATTERY_CHARGING,
    STATUS_DISPLAY_ICON_COUNT,
} status_display_icon_t;

typedef enum {
    STATUS_DISPLAY_NETWORK_OFFLINE = 0,
    STATUS_DISPLAY_NETWORK_PAIRING,
    STATUS_DISPLAY_NETWORK_JOINED,
    STATUS_DISPLAY_NETWORK_REJOINING,
} status_display_network_state_t;

typedef enum {
    STATUS_DISPLAY_POWER_EXTERNAL = 0,
    STATUS_DISPLAY_POWER_BATTERY,
} status_display_power_source_t;

esp_err_t status_display_init(void);
void status_display_set_state(status_display_state_t state);
void status_display_show_event(status_display_icon_t icon, const char *title, const char *detail, uint32_t timeout_ms);
void status_display_set_network_status(status_display_network_state_t network_state, const char *primary_state);
void status_display_set_link_quality(bool known, uint8_t lqi, int8_t rssi);
void status_display_set_power_status(status_display_power_source_t source, bool charge_known, uint8_t charge_percent, bool charging_known, bool charging);
void status_display_set_output_state(bool on);
void status_display_set_reset_progress(uint8_t percent);
void status_display_set_backlight(uint8_t level);

#ifdef __cplusplus
}
#endif
