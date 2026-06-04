#include "board_input.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BOARD_INPUT";

static board_input_config_t s_config;

static void poll_button(gpio_num_t gpio,
                        bool *was_pressed,
                        int64_t *pressed_since_ms,
                        bool *hold_start_reported,
                        bool *reset_reported)
{
    bool pressed = gpio_get_level(gpio) == 0;
    int64_t now_ms = esp_timer_get_time() / 1000;

    if (pressed && !*was_pressed) {
        *pressed_since_ms = now_ms;
        *hold_start_reported = false;
        *reset_reported = false;
    } else if (!pressed) {
        *pressed_since_ms = 0;
        *hold_start_reported = false;
        *reset_reported = false;
    }

    if (pressed && *pressed_since_ms > 0) {
        int64_t held_ms = now_ms - *pressed_since_ms;
        if (held_ms >= 1500 && !*hold_start_reported && s_config.callback) {
            *hold_start_reported = true;
            s_config.callback(BOARD_INPUT_EVENT_RESET_HOLD_START, s_config.callback_ctx);
        }
        if (held_ms >= s_config.factory_reset_hold_ms && !*reset_reported) {
            *reset_reported = true;
            if (s_config.callback) {
                s_config.callback(BOARD_INPUT_EVENT_FACTORY_RESET_REQUEST, s_config.callback_ctx);
            }
        }
    }

    *was_pressed = pressed;
}

static void input_task(void *arg)
{
    bool boot_was_pressed = false;
    bool boot_hold_start_reported = false;
    bool boot_reset_reported = false;
    int64_t boot_pressed_since_ms = 0;
    bool ext_was_pressed = false;
    bool ext_hold_start_reported = false;
    bool ext_reset_reported = false;
    int64_t ext_pressed_since_ms = 0;

    while (true) {
        poll_button((gpio_num_t)s_config.boot_gpio,
                    &boot_was_pressed,
                    &boot_pressed_since_ms,
                    &boot_hold_start_reported,
                    &boot_reset_reported);
        if (s_config.external_reset_gpio >= 0) {
            poll_button((gpio_num_t)s_config.external_reset_gpio,
                        &ext_was_pressed,
                        &ext_pressed_since_ms,
                        &ext_hold_start_reported,
                        &ext_reset_reported);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static esp_err_t configure_input(int gpio)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

esp_err_t board_input_init(const board_input_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "missing board input config");
    ESP_RETURN_ON_FALSE(config->boot_gpio >= 0, ESP_ERR_INVALID_ARG, TAG, "missing BOOT GPIO");
    ESP_RETURN_ON_FALSE(config->factory_reset_hold_ms > 0, ESP_ERR_INVALID_ARG, TAG, "missing hold time");

    s_config = *config;

    ESP_RETURN_ON_ERROR(configure_input(s_config.boot_gpio), TAG, "failed to configure BOOT GPIO");
    if (s_config.external_reset_gpio >= 0) {
        ESP_RETURN_ON_ERROR(configure_input(s_config.external_reset_gpio), TAG, "failed to configure external reset GPIO");
    }

    xTaskCreate(input_task, "board_input", 3072, NULL, 4, NULL);
    return ESP_OK;
}
