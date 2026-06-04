#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_INPUT_EVENT_RESET_HOLD_START = 0,
    BOARD_INPUT_EVENT_RESET_HOLD_PROGRESS,
    BOARD_INPUT_EVENT_RESET_HOLD_CANCELLED,
    BOARD_INPUT_EVENT_FACTORY_RESET_REQUEST,
} board_input_event_t;

typedef void (*board_input_callback_t)(board_input_event_t event, uint8_t progress, void *ctx);

typedef struct {
    int boot_gpio;
    int external_reset_gpio;
    uint32_t factory_reset_hold_ms;
    board_input_callback_t callback;
    void *callback_ctx;
} board_input_config_t;

esp_err_t board_input_init(const board_input_config_t *config);

#ifdef __cplusplus
}
#endif
