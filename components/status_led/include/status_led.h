#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    STATUS_LED_MODE_BOOT = 0,
    STATUS_LED_MODE_JOINING,
    STATUS_LED_MODE_JOINED,
    STATUS_LED_MODE_REJOINING,
    STATUS_LED_MODE_IDENTIFYING,
    STATUS_LED_MODE_RESET_HOLD,
    STATUS_LED_MODE_FACTORY_RESET,
    STATUS_LED_MODE_ERROR,
} status_led_mode_t;

esp_err_t status_led_init(void);
void status_led_startup_pulse(void);
void status_led_set_mode(status_led_mode_t mode);
void status_led_set_joined(bool output_on);
void status_led_flash_command(bool output_on);
void status_led_error_blink(void);
void status_led_set_user_light(bool on, uint8_t level);
