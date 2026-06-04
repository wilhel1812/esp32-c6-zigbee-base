idf_component_register(SRCS "esp32_c6_zigbee_base.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp-zigbee-lib
                       PRIV_REQUIRES board_input esp_timer nvs_flash status_display status_led)
