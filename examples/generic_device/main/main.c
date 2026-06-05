#include "esp32_c6_zigbee_base.h"

#include "esp_check.h"

extern const esp32_c6_zigbee_base_product_config_t generic_device_config;

void app_main(void)
{
    ESP_ERROR_CHECK(esp32_c6_zigbee_base_start_product(&generic_device_config));
}
