set(GENERATED_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_ICON_SRC "${GENERATED_ICON_DIR}/status_display_icons.c")
set(GENERATED_ICON_HEADER "${GENERATED_ICON_DIR}/status_display_icons.h")
file(MAKE_DIRECTORY "${GENERATED_ICON_DIR}")

idf_component_register(SRCS "status_display.c" "${GENERATED_ICON_SRC}"
                       INCLUDE_DIRS "include" "${GENERATED_ICON_DIR}"
                       PRIV_REQUIRES driver esp_driver_ledc esp_lcd esp_timer)

if(NOT CMAKE_SCRIPT_MODE_FILE)
    find_program(NODE_EXECUTABLE NAMES node)
    if(NOT NODE_EXECUTABLE)
        message(FATAL_ERROR "node is required to generate Lucide LCD icon assets")
    endif()

    add_custom_command(
        OUTPUT "${GENERATED_ICON_SRC}" "${GENERATED_ICON_HEADER}"
        COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../scripts/generate_lucide_assets.js" "${GENERATED_ICON_DIR}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../scripts/generate_lucide_assets.js"
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
        COMMENT "Generating exact Lucide LCD icon assets"
        VERBATIM)
endif()
