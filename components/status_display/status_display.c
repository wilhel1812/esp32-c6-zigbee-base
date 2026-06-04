#include "status_display.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "status_display_icons.h"

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
#define LCD_CHUNK_ROWS LCD_HEIGHT
#define LCD_STATUS_BAR_HEIGHT 24
#define LCD_BL_LEDC_TIMER LEDC_TIMER_0
#define LCD_BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define LCD_BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_BL_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define LCD_BL_LEDC_MAX_DUTY ((1 << 10) - 1)
#define LCD_BL_LEDC_FREQ_HZ 5000
#define DISPLAY_TEXT_MAX 32
#define DISPLAY_EVENT_DEFAULT_MS 3000

typedef struct {
    status_display_icon_t icon;
    const char *title;
    const char *detail;
    uint16_t bg;
    uint16_t accent;
    uint16_t fg;
} display_view_t;

typedef struct {
    bool active;
    status_display_icon_t icon;
    char title[DISPLAY_TEXT_MAX];
    char detail[DISPLAY_TEXT_MAX];
    int64_t expires_us;
} display_event_t;

typedef struct {
    status_display_state_t state;
    status_display_network_state_t network_state;
    bool link_known;
    uint8_t link_lqi;
    int8_t link_rssi;
    status_display_power_source_t power_source;
    bool charge_known;
    uint8_t charge_percent;
    bool charging_known;
    bool charging;
    bool output_on;
    uint8_t reset_progress;
    char primary_state[DISPLAY_TEXT_MAX];
    display_event_t event;
} display_model_t;

static const char *TAG = "STATUS_DISPLAY";
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_flush_done;
static TaskHandle_t s_task;
static uint16_t *s_frame;
static display_model_t s_model = {
    .state = STATUS_DISPLAY_BOOTING,
    .network_state = STATUS_DISPLAY_NETWORK_OFFLINE,
    .link_known = false,
    .link_lqi = 0,
    .link_rssi = 0,
    .power_source = STATUS_DISPLAY_POWER_EXTERNAL,
    .charge_known = false,
    .charge_percent = 0,
    .charging_known = false,
    .charging = false,
    .output_on = false,
    .reset_progress = 0,
    .primary_state = "BOOTING",
};
static uint32_t s_render_generation;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

static bool display_flush_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_woken = pdFALSE;
    if (s_flush_done) {
        xSemaphoreGiveFromISR(s_flush_done, &high_task_woken);
    }
    return high_task_woken == pdTRUE;
}

static uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t alpha)
{
    uint8_t br = ((bg >> 11) & 0x1f) << 3;
    uint8_t bgc = ((bg >> 5) & 0x3f) << 2;
    uint8_t bb = (bg & 0x1f) << 3;
    uint8_t fr = ((fg >> 11) & 0x1f) << 3;
    uint8_t fgc = ((fg >> 5) & 0x3f) << 2;
    uint8_t fb = (fg & 0x1f) << 3;
    uint8_t r = (br * (255 - alpha) + fr * alpha) / 255;
    uint8_t g = (bgc * (255 - alpha) + fgc * alpha) / 255;
    uint8_t b = (bb * (255 - alpha) + fb * alpha) / 255;
    return rgb565(r, g, b);
}

static void copy_text(char *dest, const char *value)
{
    if (!value || !value[0]) {
        dest[0] = '\0';
        return;
    }
    strlcpy(dest, value, DISPLAY_TEXT_MAX);
}

static display_view_t view_for_state(const display_model_t *model)
{
    switch (model->state) {
    case STATUS_DISPLAY_PAIRING:
        return (display_view_t){STATUS_DISPLAY_ICON_RADIO_TOWER, "PAIRING", "PERMIT JOIN", rgb565(1, 13, 34), rgb565(70, 160, 255), rgb565(242, 248, 255)};
    case STATUS_DISPLAY_JOINED_ON:
        return (display_view_t){STATUS_DISPLAY_ICON_POWER, "OUTLET ON", "", rgb565(0, 28, 14), rgb565(78, 230, 138), rgb565(236, 255, 244)};
    case STATUS_DISPLAY_STANDBY:
        return (display_view_t){STATUS_DISPLAY_ICON_POWER, "OUTLET OFF", "", rgb565(4, 24, 16), rgb565(66, 190, 112), rgb565(232, 250, 238)};
    case STATUS_DISPLAY_IDENTIFYING:
        return (display_view_t){STATUS_DISPLAY_ICON_LOCATE_FIXED, "IDENTIFY", "", rgb565(44, 36, 0), rgb565(255, 214, 72), rgb565(255, 252, 210)};
    case STATUS_DISPLAY_REJOINING:
        return (display_view_t){STATUS_DISPLAY_ICON_REFRESH_CW, "REJOINING", "NETWORK LOST", rgb565(46, 24, 0), rgb565(255, 165, 55), rgb565(255, 238, 210)};
    case STATUS_DISPLAY_RESET_HOLD:
        return (display_view_t){STATUS_DISPLAY_ICON_ROTATE_CCW, "HOLD RESET", "KEEP HOLDING", rgb565(56, 22, 0), rgb565(255, 130, 55), rgb565(255, 232, 210)};
    case STATUS_DISPLAY_FACTORY_RESETTING:
        return (display_view_t){STATUS_DISPLAY_ICON_ROTATE_CCW, "RESETTING", "LEAVING NETWORK", rgb565(42, 0, 48), rgb565(235, 110, 255), rgb565(255, 226, 255)};
    case STATUS_DISPLAY_ERROR:
        return (display_view_t){STATUS_DISPLAY_ICON_TRIANGLE_ALERT, "ERROR", "CHECK DEVICE", rgb565(58, 0, 0), rgb565(255, 82, 82), rgb565(255, 226, 226)};
    case STATUS_DISPLAY_BOOTING:
    default:
        return (display_view_t){STATUS_DISPLAY_ICON_REFRESH_CW, "BOOTING", "STARTING", rgb565(24, 26, 30), rgb565(180, 190, 205), rgb565(250, 250, 250)};
    }
}

static uint8_t font_column(char c, uint8_t col)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    const uint8_t *glyph = blank;

    switch (c) {
    case '0': { static const uint8_t g[5] = {0x3e, 0x51, 0x49, 0x45, 0x3e}; glyph = g; } break;
    case '1': { static const uint8_t g[5] = {0x00, 0x42, 0x7f, 0x40, 0x00}; glyph = g; } break;
    case '2': { static const uint8_t g[5] = {0x62, 0x51, 0x49, 0x49, 0x46}; glyph = g; } break;
    case '3': { static const uint8_t g[5] = {0x22, 0x41, 0x49, 0x49, 0x36}; glyph = g; } break;
    case '4': { static const uint8_t g[5] = {0x18, 0x14, 0x12, 0x7f, 0x10}; glyph = g; } break;
    case '5': { static const uint8_t g[5] = {0x2f, 0x49, 0x49, 0x49, 0x31}; glyph = g; } break;
    case '6': { static const uint8_t g[5] = {0x3e, 0x49, 0x49, 0x49, 0x32}; glyph = g; } break;
    case '7': { static const uint8_t g[5] = {0x01, 0x71, 0x09, 0x05, 0x03}; glyph = g; } break;
    case '8': { static const uint8_t g[5] = {0x36, 0x49, 0x49, 0x49, 0x36}; glyph = g; } break;
    case '9': { static const uint8_t g[5] = {0x26, 0x49, 0x49, 0x49, 0x3e}; glyph = g; } break;
    case 'A': { static const uint8_t g[5] = {0x7e, 0x11, 0x11, 0x11, 0x7e}; glyph = g; } break;
    case 'B': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x36}; glyph = g; } break;
    case 'C': { static const uint8_t g[5] = {0x3e, 0x41, 0x41, 0x41, 0x22}; glyph = g; } break;
    case 'D': { static const uint8_t g[5] = {0x7f, 0x41, 0x41, 0x22, 0x1c}; glyph = g; } break;
    case 'E': { static const uint8_t g[5] = {0x7f, 0x49, 0x49, 0x49, 0x41}; glyph = g; } break;
    case 'F': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x01}; glyph = g; } break;
    case 'G': { static const uint8_t g[5] = {0x3e, 0x41, 0x49, 0x49, 0x7a}; glyph = g; } break;
    case 'H': { static const uint8_t g[5] = {0x7f, 0x08, 0x08, 0x08, 0x7f}; glyph = g; } break;
    case 'I': { static const uint8_t g[5] = {0x00, 0x41, 0x7f, 0x41, 0x00}; glyph = g; } break;
    case 'J': { static const uint8_t g[5] = {0x20, 0x40, 0x41, 0x3f, 0x01}; glyph = g; } break;
    case 'K': { static const uint8_t g[5] = {0x7f, 0x08, 0x14, 0x22, 0x41}; glyph = g; } break;
    case 'L': { static const uint8_t g[5] = {0x7f, 0x40, 0x40, 0x40, 0x40}; glyph = g; } break;
    case 'M': { static const uint8_t g[5] = {0x7f, 0x02, 0x0c, 0x02, 0x7f}; glyph = g; } break;
    case 'N': { static const uint8_t g[5] = {0x7f, 0x02, 0x0c, 0x10, 0x7f}; glyph = g; } break;
    case 'O': { static const uint8_t g[5] = {0x3e, 0x41, 0x41, 0x41, 0x3e}; glyph = g; } break;
    case 'P': { static const uint8_t g[5] = {0x7f, 0x09, 0x09, 0x09, 0x06}; glyph = g; } break;
    case 'Q': { static const uint8_t g[5] = {0x3e, 0x41, 0x51, 0x21, 0x5e}; glyph = g; } break;
    case 'R': { static const uint8_t g[5] = {0x7f, 0x09, 0x19, 0x29, 0x46}; glyph = g; } break;
    case 'S': { static const uint8_t g[5] = {0x46, 0x49, 0x49, 0x49, 0x31}; glyph = g; } break;
    case 'T': { static const uint8_t g[5] = {0x01, 0x01, 0x7f, 0x01, 0x01}; glyph = g; } break;
    case 'U': { static const uint8_t g[5] = {0x3f, 0x40, 0x40, 0x40, 0x3f}; glyph = g; } break;
    case 'V': { static const uint8_t g[5] = {0x1f, 0x20, 0x40, 0x20, 0x1f}; glyph = g; } break;
    case 'W': { static const uint8_t g[5] = {0x7f, 0x20, 0x18, 0x20, 0x7f}; glyph = g; } break;
    case 'X': { static const uint8_t g[5] = {0x63, 0x14, 0x08, 0x14, 0x63}; glyph = g; } break;
    case 'Y': { static const uint8_t g[5] = {0x07, 0x08, 0x70, 0x08, 0x07}; glyph = g; } break;
    case 'Z': { static const uint8_t g[5] = {0x61, 0x51, 0x49, 0x45, 0x43}; glyph = g; } break;
    case ':': { static const uint8_t g[5] = {0x00, 0x36, 0x36, 0x00, 0x00}; glyph = g; } break;
    case '%': { static const uint8_t g[5] = {0x23, 0x13, 0x08, 0x64, 0x62}; glyph = g; } break;
    case '-': { static const uint8_t g[5] = {0x08, 0x08, 0x08, 0x08, 0x08}; glyph = g; } break;
    case '.': { static const uint8_t g[5] = {0x00, 0x60, 0x60, 0x00, 0x00}; glyph = g; } break;
    default:
        break;
    }

    return col < 5 ? glyph[col] : 0;
}

static char normalize_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

static void draw_pixel(uint16_t *frame, int chunk_y, int x, int y, uint16_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= chunk_y && y < chunk_y + LCD_CHUNK_ROWS) {
        frame[(y - chunk_y) * LCD_WIDTH + x] = color;
    }
}

static void draw_rect(uint16_t *frame, int chunk_y, int x, int y, int w, int h, uint16_t color)
{
    int y0 = y > chunk_y ? y : chunk_y;
    int y1 = y + h < chunk_y + LCD_CHUNK_ROWS ? y + h : chunk_y + LCD_CHUNK_ROWS;
    for (int py = y0; py < y1; py++) {
        for (int px = x; px < x + w; px++) {
            draw_pixel(frame, chunk_y, px, py, color);
        }
    }
}

static int text_width(const char *text, int scale)
{
    int len = text ? strlen(text) : 0;
    return len > 0 ? len * 6 * scale - scale : 0;
}

static void draw_char(uint16_t *frame, int chunk_y, int x, int y, char c, uint16_t color, int scale)
{
    c = normalize_char(c);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = font_column(c, col);
        for (int row = 0; row < 7; row++) {
            if ((bits & (1 << row)) == 0) {
                continue;
            }
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    draw_pixel(frame, chunk_y, x + col * scale + dx, y + row * scale + dy, color);
                }
            }
        }
    }
}

static void draw_text(uint16_t *frame, int chunk_y, int x, int y, const char *text, uint16_t color, int scale)
{
    if (!text) {
        return;
    }
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(frame, chunk_y, x + i * 6 * scale, y, text[i], color, scale);
    }
}

static void draw_centered_text(uint16_t *frame, int chunk_y, const char *text, int y, uint16_t color, int scale)
{
    int width = text_width(text, scale);
    int x = (LCD_WIDTH - width) / 2;
    draw_text(frame, chunk_y, x, y, text, color, scale);
}

static void draw_clipped_text(uint16_t *frame, int chunk_y, int x, int y, const char *text, int max_width, uint16_t color, int scale)
{
    char buf[DISPLAY_TEXT_MAX];
    copy_text(buf, text);
    while (buf[0] && text_width(buf, scale) > max_width) {
        size_t len = strlen(buf);
        if (len <= 1) {
            break;
        }
        buf[len - 1] = '\0';
    }
    draw_text(frame, chunk_y, x, y, buf, color, scale);
}

static void draw_icon(uint16_t *frame, int chunk_y, status_display_icon_t icon, int x, int y, bool large, uint16_t color)
{
    const status_display_icon_asset_t *asset = large ? status_display_icon_get_88(icon) : status_display_icon_get_16(icon);
    if (!asset) {
        return;
    }

    int size = asset->size;
    for (int py = 0; py < size; py++) {
        int screen_y = y + py;
        if (screen_y < chunk_y || screen_y >= chunk_y + LCD_CHUNK_ROWS) {
            continue;
        }
        for (int px = 0; px < size; px++) {
            int pixel = py * size + px;
            uint8_t packed = asset->alpha4[pixel >> 1];
            uint8_t alpha4 = (pixel & 1) ? (packed >> 4) : (packed & 0x0f);
            if (alpha4 != 0) {
                int screen_x = x + px;
                if (screen_x >= 0 && screen_x < LCD_WIDTH) {
                    uint16_t bg = frame[(screen_y - chunk_y) * LCD_WIDTH + screen_x];
                    draw_pixel(frame, chunk_y, screen_x, screen_y, blend565(bg, color, alpha4 * 17));
                }
            }
        }
    }
}

static status_display_icon_t status_icon(const display_model_t *model)
{
    switch (model->state) {
    case STATUS_DISPLAY_IDENTIFYING:
        return STATUS_DISPLAY_ICON_LOCATE_FIXED;
    case STATUS_DISPLAY_REJOINING:
        return STATUS_DISPLAY_ICON_REFRESH_CW;
    case STATUS_DISPLAY_RESET_HOLD:
    case STATUS_DISPLAY_FACTORY_RESETTING:
        return STATUS_DISPLAY_ICON_ROTATE_CCW;
    case STATUS_DISPLAY_ERROR:
        return STATUS_DISPLAY_ICON_TRIANGLE_ALERT;
    default:
        break;
    }

    switch (model->network_state) {
    case STATUS_DISPLAY_NETWORK_PAIRING:
        return STATUS_DISPLAY_ICON_RADIO_TOWER;
    case STATUS_DISPLAY_NETWORK_JOINED:
        return STATUS_DISPLAY_ICON_WIFI;
    case STATUS_DISPLAY_NETWORK_REJOINING:
    case STATUS_DISPLAY_NETWORK_OFFLINE:
    default:
        return STATUS_DISPLAY_ICON_WIFI_OFF;
    }
}

static uint8_t link_bars(uint8_t lqi)
{
    if (lqi >= 170) {
        return 3;
    }
    if (lqi >= 85) {
        return 2;
    }
    if (lqi > 0) {
        return 1;
    }
    return 0;
}

static status_display_icon_t signal_icon(uint8_t bars)
{
    if (bars >= 3) {
        return STATUS_DISPLAY_ICON_SIGNAL_HIGH;
    }
    if (bars == 2) {
        return STATUS_DISPLAY_ICON_SIGNAL_MEDIUM;
    }
    if (bars == 1) {
        return STATUS_DISPLAY_ICON_SIGNAL_LOW;
    }
    return STATUS_DISPLAY_ICON_SIGNAL_ZERO;
}

static bool should_draw_link(const display_model_t *model)
{
    return model->network_state == STATUS_DISPLAY_NETWORK_JOINED && model->link_known && link_bars(model->link_lqi) > 0;
}

static bool should_draw_battery(const display_model_t *model)
{
    return model->power_source == STATUS_DISPLAY_POWER_BATTERY && model->charge_known;
}

static void draw_status_bar(uint16_t *frame, int chunk_y, const display_model_t *model, uint16_t bg, uint16_t fg, uint16_t accent)
{
    uint16_t bar_bg = blend565(bg, rgb565(0, 0, 0), 80);
    uint16_t dim = blend565(bg, fg, 120);
    bool draw_link = should_draw_link(model);
    bool draw_battery = should_draw_battery(model);
    int text_max = 154;
    draw_rect(frame, chunk_y, 0, 0, LCD_WIDTH, LCD_STATUS_BAR_HEIGHT, bar_bg);
    draw_icon(frame, chunk_y, status_icon(model), 6, 4, false, accent);

    if (draw_link) {
        text_max -= 22;
    }
    if (draw_battery) {
        text_max -= 46;
    }
    if (text_max < 72) {
        text_max = 72;
    }
    draw_clipped_text(frame, chunk_y, 30, 7, model->primary_state, text_max, fg, 1);

    if (draw_link) {
        draw_icon(frame, chunk_y, signal_icon(link_bars(model->link_lqi)), draw_battery ? 170 : 216, 4, false, dim);
    }
    if (draw_battery) {
        char charge[5];
        snprintf(charge, sizeof(charge), "%u%%", model->charge_percent);
        draw_clipped_text(frame, chunk_y, 192, 7, charge, 24, fg, 1);
        draw_icon(frame, chunk_y,
                  model->charging_known && model->charging ? STATUS_DISPLAY_ICON_BATTERY_CHARGING : STATUS_DISPLAY_ICON_BATTERY,
                  220, 4, false, dim);
    }
}

static void draw_reset_progress(uint16_t *frame, int chunk_y, uint8_t progress, uint16_t bg, uint16_t accent)
{
    int width = 132;
    int filled = width * progress / 100;
    int x = (LCD_WIDTH - width) / 2;
    int y = 202;
    draw_rect(frame, chunk_y, x, y, width, 6, blend565(bg, accent, 70));
    draw_rect(frame, chunk_y, x, y, filled, 6, accent);
}

static void render_view(uint16_t *frame, int chunk_y, const display_model_t *model, display_view_t view)
{
    draw_status_bar(frame, chunk_y, model, view.bg, view.fg, view.accent);
    draw_icon(frame, chunk_y, view.icon, 76, 48, true, view.accent);
    draw_centered_text(frame, chunk_y, view.title, 150, view.fg, 2);
    if (view.detail && view.detail[0]) {
        draw_centered_text(frame, chunk_y, view.detail, 178, blend565(view.bg, view.fg, 210), 1);
    }
    if (model->state == STATUS_DISPLAY_RESET_HOLD) {
        draw_reset_progress(frame, chunk_y, model->reset_progress, view.bg, view.accent);
    }
}

static void render_state(const display_model_t *model)
{
    display_view_t view = view_for_state(model);
    if (model->event.active) {
        view.icon = model->event.icon;
        view.title = model->event.title;
        view.detail = model->event.detail;
        view.bg = rgb565(0, 28, 36);
        view.accent = rgb565(74, 224, 255);
        view.fg = rgb565(232, 254, 255);
    }

    if (!s_frame) {
        ESP_LOGW(TAG, "LCD frame buffer not allocated");
        return;
    }

    xSemaphoreTake(s_flush_done, portMAX_DELAY);

    for (int y = 0; y < LCD_HEIGHT; y += LCD_CHUNK_ROWS) {
        for (size_t i = 0; i < LCD_WIDTH * LCD_CHUNK_ROWS; i++) {
            s_frame[i] = view.bg;
        }
        render_view(s_frame, y, model, view);
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_WIDTH, y + LCD_CHUNK_ROWS, s_frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "failed to draw LCD frame: %s", esp_err_to_name(err));
            xSemaphoreGive(s_flush_done);
        }
    }
}

static void display_task(void *arg)
{
    display_model_t rendered = s_model;
    uint32_t rendered_generation = 0;
    render_state(&rendered);

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
        vTaskDelay(pdMS_TO_TICKS(40));
        ulTaskNotifyTake(pdTRUE, 0);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_model.event.active && esp_timer_get_time() >= s_model.event.expires_us) {
            s_model.event.active = false;
            s_render_generation++;
        }
        display_model_t current = s_model;
        uint32_t current_generation = s_render_generation;
        xSemaphoreGive(s_lock);

        if (memcmp(&current, &rendered, sizeof(current)) != 0 || current_generation != rendered_generation) {
            rendered = current;
            rendered_generation = current_generation;
            render_state(&rendered);
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
        .on_color_trans_done = display_flush_done_cb,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
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
    s_flush_done = xSemaphoreCreateBinary();
    if (!s_flush_done) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_flush_done);
    s_frame = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_frame) {
        ESP_LOGW(TAG, "failed to allocate LCD frame buffer");
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
    s_model.state = state;
    if (state != STATUS_DISPLAY_RESET_HOLD) {
        s_model.reset_progress = 0;
    }
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_show_event(status_display_icon_t icon, const char *title, const char *detail, uint32_t timeout_ms)
{
    if (!s_lock) {
        return;
    }

    if (timeout_ms == 0) {
        timeout_ms = DISPLAY_EVENT_DEFAULT_MS;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.event.active = true;
    s_model.event.icon = icon;
    copy_text(s_model.event.title, title);
    copy_text(s_model.event.detail, detail);
    s_model.event.expires_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_network_status(status_display_network_state_t network_state, const char *primary_state)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.network_state = network_state;
    if (network_state != STATUS_DISPLAY_NETWORK_JOINED) {
        s_model.link_known = false;
        s_model.link_lqi = 0;
        s_model.link_rssi = 0;
    }
    copy_text(s_model.primary_state, primary_state);
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_link_quality(bool known, uint8_t lqi, int8_t rssi)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.link_known = known && lqi > 0;
    s_model.link_lqi = s_model.link_known ? lqi : 0;
    s_model.link_rssi = s_model.link_known ? rssi : 0;
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_power_status(status_display_power_source_t source, bool charge_known, uint8_t charge_percent, bool charging_known, bool charging)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.power_source = source;
    s_model.charge_known = source == STATUS_DISPLAY_POWER_BATTERY && charge_known;
    s_model.charge_percent = charge_percent > 100 ? 100 : charge_percent;
    s_model.charging_known = source == STATUS_DISPLAY_POWER_BATTERY && charging_known;
    s_model.charging = s_model.charging_known && charging;
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_output_state(bool on)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.output_on = on;
    s_model.state = on ? STATUS_DISPLAY_JOINED_ON : STATUS_DISPLAY_STANDBY;
    s_render_generation++;
    xSemaphoreGive(s_lock);

    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

void status_display_set_reset_progress(uint8_t percent)
{
    if (!s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.reset_progress = percent > 100 ? 100 : percent;
    s_render_generation++;
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
