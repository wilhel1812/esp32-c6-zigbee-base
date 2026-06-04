#include "status_display.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LCD_HOST SPI2_HOST
#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define LCD_PIN_MOSI 6
#define LCD_PIN_CLK 7
#define LCD_PIN_DC 15
#define LCD_PIN_CS 14
#define LCD_PIN_RST 21
#define LCD_PIN_BL 22
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_CHUNK_ROWS 20
#define LCD_BL_LEDC_TIMER LEDC_TIMER_0
#define LCD_BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define LCD_BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_BL_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define LCD_BL_LEDC_MAX_DUTY ((1 << 10) - 1)
#define LCD_BL_LEDC_FREQ_HZ 5000

typedef struct {
    const char *label;
    uint16_t bg;
    uint16_t fg;
} display_view_t;

static const char *TAG = "STATUS_DISPLAY";
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static status_display_state_t s_state = STATUS_DISPLAY_BOOTING;
static uint32_t s_render_generation;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

static display_view_t view_for_state(status_display_state_t state)
{
    switch (state) {
    case STATUS_DISPLAY_PAIRING:
        return (display_view_t){"PAIRING", rgb565(0, 16, 64), rgb565(240, 248, 255)};
    case STATUS_DISPLAY_JOINED_OFF:
        return (display_view_t){"JOINED OFF", rgb565(0, 36, 14), rgb565(220, 255, 230)};
    case STATUS_DISPLAY_JOINED_ON:
        return (display_view_t){"JOINED ON", rgb565(0, 72, 24), rgb565(235, 255, 240)};
    case STATUS_DISPLAY_IDENTIFYING:
        return (display_view_t){"IDENTIFY", rgb565(88, 72, 0), rgb565(255, 255, 200)};
    case STATUS_DISPLAY_REJOINING:
        return (display_view_t){"REJOINING", rgb565(64, 32, 0), rgb565(255, 238, 190)};
    case STATUS_DISPLAY_RESET_HOLD:
        return (display_view_t){"RESET HOLD", rgb565(88, 32, 0), rgb565(255, 230, 200)};
    case STATUS_DISPLAY_FACTORY_RESETTING:
        return (display_view_t){"RESETTING", rgb565(72, 0, 72), rgb565(255, 220, 255)};
    case STATUS_DISPLAY_ERROR:
        return (display_view_t){"ERROR", rgb565(80, 0, 0), rgb565(255, 220, 220)};
    case STATUS_DISPLAY_BOOTING:
    default:
        return (display_view_t){"BOOTING", rgb565(42, 42, 42), rgb565(255, 255, 255)};
    }
}

static uint8_t font_column(char c, uint8_t col)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    const uint8_t *glyph = blank;

    switch (c) {
    case 'A': { static const uint8_t g[5] = {0x7e, 0x11, 0x11, 0x11, 0x7e}; glyph = g; } break;
    case 'B': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x36}; glyph = g; } break;
    case 'D': { static const uint8_t g[5] = {0x7f, 0x41, 0x41, 0x22, 0x1c}; glyph = g; } break;
    case 'E': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x41}; glyph = g; } break;
    case 'F': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x01}; glyph = g; } break;
    case 'G': { static const uint8_t g[5] = {0x3e, 0x41, 0x49, 0x49, 0x7a}; glyph = g; } break;
    case 'H': { static const uint8_t g[5] = {0x7f, 0x08, 0x08, 0x08, 0x7f}; glyph = g; } break;
    case 'I': { static const uint8_t g[5] = {0x00, 0x41, 0x7f, 0x41, 0x00}; glyph = g; } break;
    case 'J': { static const uint8_t g[5] = {0x20, 0x40, 0x41, 0x3f, 0x01}; glyph = g; } break;
    case 'L': { static const uint8_t g[5] = {0x7f, 0x40, 0x40, 0x40, 0x40}; glyph = g; } break;
    case 'N': { static const uint8_t g[5] = {0x7f, 0x02, 0x0c, 0x10, 0x7f}; glyph = g; } break;
    case 'O': { static const uint8_t g[5] = {0x3e, 0x41, 0x41, 0x41, 0x3e}; glyph = g; } break;
    case 'P': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x06}; glyph = g; } break;
    case 'R': { static const uint8_t g[5] = {0x7f, 0x09, 0x19, 0x29, 0x46}; glyph = g; } break;
    case 'S': { static const uint8_t g[5] = {0x46, 0x49, 0x49, 0x49, 0x31}; glyph = g; } break;
    case 'T': { static const uint8_t g[5] = {0x01, 0x01, 0x7f, 0x01, 0x01}; glyph = g; } break;
    case 'Y': { static const uint8_t g[5] = {0x07, 0x08, 0x70, 0x08, 0x07}; glyph = g; } break;
    default:
        break;
    }

    return col < 5 ? glyph[col] : 0;
}

static void draw_char(uint16_t *frame, int chunk_y, int x, int y, char c, uint16_t color, int scale)
{
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font_column(c, col);
        for (int row = 0; row < 7; row++) {
            if ((bits & (1 << row)) == 0) {
                continue;
            }
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int px = x + col * scale + dx;
                    int py = y + row * scale + dy;
                    if (px >= 0 && px < LCD_WIDTH && py >= chunk_y && py < chunk_y + LCD_CHUNK_ROWS) {
                        frame[(py - chunk_y) * LCD_WIDTH + px] = color;
                    }
                }
            }
        }
    }
}

static void draw_centered_text(uint16_t *frame, int chunk_y, const char *text, uint16_t color)
{
    int scale = 4;
    int len = strlen(text);
    int width = len * 6 * scale - scale;
    int x = (LCD_WIDTH - width) / 2;
    int y = (LCD_HEIGHT - 7 * scale) / 2;

    for (int i = 0; i < len; i++) {
        draw_char(frame, chunk_y, x + i * 6 * scale, y, text[i], color, scale);
    }
}

static void render_state(status_display_state_t state)
{
    display_view_t view = view_for_state(state);
    uint16_t *frame = heap_caps_malloc(LCD_WIDTH * LCD_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!frame) {
        ESP_LOGW(TAG, "failed to allocate LCD frame chunk");
        return;
    }

    for (int y = 0; y < LCD_HEIGHT; y += LCD_CHUNK_ROWS) {
        for (size_t i = 0; i < LCD_WIDTH * LCD_CHUNK_ROWS; i++) {
            frame[i] = view.bg;
        }
        draw_centered_text(frame, y, view.label, view.fg);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_WIDTH, y + LCD_CHUNK_ROWS, frame));
    }

    free(frame);
}

static void display_task(void *arg)
{
    status_display_state_t rendered = STATUS_DISPLAY_BOOTING;
    uint32_t rendered_generation = 0;
    render_state(rendered);

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(80));
        ulTaskNotifyTake(pdTRUE, 0);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        status_display_state_t current = s_state;
        uint32_t current_generation = s_render_generation;
        xSemaphoreGive(s_lock);

        if (current != rendered || current_generation != rendered_generation) {
            rendered = current;
            rendered_generation = current_generation;
            render_state(rendered);
        }
    }
}

esp_err_t status_display_init(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_CHUNK_ROWS * sizeof(uint16_t),
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ledc_timer_config_t bl_timer = {
        .speed_mode = LCD_BL_LEDC_MODE,
        .duty_resolution = LCD_BL_LEDC_DUTY_RES,
        .timer_num = LCD_BL_LEDC_TIMER,
        .freq_hz = LCD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t bl_channel = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LCD_BL_LEDC_MODE,
        .channel = LCD_BL_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LCD_BL_LEDC_TIMER,
        .duty = LCD_BL_LEDC_MAX_DUTY,
        .hpoint = 0,
    };

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(ledc_timer_config(&bl_timer), TAG, "failed to configure LCD backlight timer");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&bl_channel), TAG, "failed to configure LCD backlight channel");
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "failed to initialize LCD SPI bus");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle),
                        TAG, "failed to create LCD IO");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel), TAG, "failed to create ST7789 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "failed to reset LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "failed to initialize LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "failed to enable LCD");

    xTaskCreate(display_task, "status_display", 4096, NULL, 3, &s_task);
    return ESP_OK;
}

void status_display_set_state(status_display_state_t state)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state = state;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_backlight(uint8_t level)
{
    uint32_t duty = (uint32_t)level * LCD_BL_LEDC_MAX_DUTY / 0xff;

    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL));
}
