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
    STATUS_DISPLAY_JOINED_OFF,
    STATUS_DISPLAY_JOINED_ON,
    STATUS_DISPLAY_IDENTIFYING,
    STATUS_DISPLAY_REJOINING,
    STATUS_DISPLAY_RESET_HOLD,
    STATUS_DISPLAY_FACTORY_RESETTING,
    STATUS_DISPLAY_ERROR,
} status_display_state_t;

esp_err_t status_display_init(void);
void status_display_set_state(status_display_state_t state);
void status_display_set_backlight(uint8_t level);

#ifdef __cplusplus
}
#endif
