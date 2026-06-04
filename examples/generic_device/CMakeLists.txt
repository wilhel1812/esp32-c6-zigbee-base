cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

list(PREPEND SDKCONFIG_DEFAULTS "sdkconfig.mbedtls_minimal" "sdkconfig.defaults")

set(EXTRA_COMPONENT_DIRS
    "../../components/board_input"
    "../../components/status_display"
    "../../components/status_led"
    "../../components/esp32_c6_zigbee_base")

project(generic_zigbee_base)
