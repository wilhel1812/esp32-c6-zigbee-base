#include "esp32_c6_zigbee_base.h"

#include "esp_check.h"

void app_main(void)
{
    esp32_c6_zigbee_base_config_t config = ESP32_C6_ZIGBEE_BASE_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp32_c6_zigbee_base_start(&config));
}
