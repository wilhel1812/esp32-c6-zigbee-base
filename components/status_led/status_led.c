#include "status_led.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define STATUS_LED_GPIO 8
#define STATUS_LED_COUNT 1

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

static const char *TAG = "STATUS_LED";

static led_strip_handle_t s_led_strip;
static SemaphoreHandle_t s_lock;
static status_led_mode_t s_mode = STATUS_LED_MODE_BOOT;
static bool s_output_on;
static uint8_t s_flash_ticks;
static uint8_t s_error_ticks;
static rgb_t s_flash_color;
static bool s_user_light_on = true;
static uint8_t s_user_light_level = 0xff;

static rgb_t scale_pixel(rgb_t color, bool on, uint8_t level)
{
    if (!on || level == 0) {
        return (rgb_t){0, 0, 0};
    }

    return (rgb_t){
        .red = (uint16_t)color.red * level / 0xff,
        .green = (uint16_t)color.green * level / 0xff,
        .blue = (uint16_t)color.blue * level / 0xff,
    };
}

static void set_pixel(rgb_t color)
{
    if (!s_led_strip) {
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_set_pixel(s_led_strip, 0, color.red, color.green, color.blue));
    ESP_ERROR_CHECK_WITHOUT_ABORT(led_strip_refresh(s_led_strip));
}

static void led_task(void *arg)
{
    uint32_t tick = 0;

    while (true) {
        status_led_mode_t mode;
        bool output_on;
        uint8_t flash_ticks;
        uint8_t error_ticks;
        rgb_t flash_color;
        bool user_light_on;
        uint8_t user_light_level;
        rgb_t color;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        mode = s_mode;
        output_on = s_output_on;
        flash_ticks = s_flash_ticks;
        error_ticks = s_error_ticks;
        flash_color = s_flash_color;
        user_light_on = s_user_light_on;
        user_light_level = s_user_light_level;
        if (s_error_ticks > 0) {
            s_error_ticks--;
        } else if (s_flash_ticks > 0) {
            s_flash_ticks--;
        }
        xSemaphoreGive(s_lock);

        if (error_ticks > 0) {
            uint8_t phase = (12 - error_ticks) % 4;
            color = phase < 2 ? (rgb_t){48, 0, 0} : (rgb_t){0, 0, 0};
        } else if (flash_ticks > 0) {
            color = flash_color;
        } else if (mode == STATUS_LED_MODE_JOINING) {
            color = (tick % 2) == 0 ? (rgb_t){0, 0, 32} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_REJOINING) {
            color = (tick % 4) < 2 ? (rgb_t){28, 12, 0} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_IDENTIFYING) {
            color = (tick % 2) == 0 ? (rgb_t){48, 48, 0} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_RESET_HOLD) {
            color = (tick % 2) == 0 ? (rgb_t){48, 12, 0} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_FACTORY_RESET) {
            color = (tick % 2) == 0 ? (rgb_t){48, 0, 48} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_ERROR) {
            color = (tick % 2) == 0 ? (rgb_t){48, 0, 0} : (rgb_t){0, 0, 0};
        } else if (mode == STATUS_LED_MODE_JOINED && output_on) {
            color = (rgb_t){0, 48, 0};
        } else if (mode == STATUS_LED_MODE_JOINED) {
            uint8_t level = (tick % 8) == 0 ? 18 : 3;
            color = (rgb_t){0, level, 0};
        } else {
            color = (rgb_t){0, 0, 0};
        }
        set_pixel(scale_pixel(color, user_light_on, user_light_level));

        tick++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t status_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = STATUS_LED_GPIO,
        .max_leds = STATUS_LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip), TAG,
                        "failed to initialize WS2812 LED");
    ESP_RETURN_ON_ERROR(led_strip_clear(s_led_strip), TAG, "failed to clear WS2812 LED");

    xTaskCreate(led_task, "status_led", 3072, NULL, 4, NULL);
    return ESP_OK;
}

void status_led_startup_pulse(void)
{
    set_pixel((rgb_t){48, 48, 48});
    vTaskDelay(pdMS_TO_TICKS(150));
    set_pixel((rgb_t){0, 0, 0});
}

void status_led_set_mode(status_led_mode_t mode)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_mode = mode;
    xSemaphoreGive(s_lock);
}

void status_led_set_joined(bool output_on)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_mode = STATUS_LED_MODE_JOINED;
    s_output_on = output_on;
    xSemaphoreGive(s_lock);
}

void status_led_flash_command(bool output_on)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_mode = STATUS_LED_MODE_JOINED;
    s_output_on = output_on;
    s_flash_color = (rgb_t){0, 48, 48};
    s_flash_ticks = 2;
    s_error_ticks = 0;
    xSemaphoreGive(s_lock);
}

void status_led_error_blink(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_flash_ticks = 0;
    s_error_ticks = 12;
    xSemaphoreGive(s_lock);
}

void status_led_set_user_light(bool on, uint8_t level)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_user_light_on = on;
    s_user_light_level = level;
    xSemaphoreGive(s_lock);
}
