cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_board"
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_i2c"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_esp32wroom/oled_ssd1315_final"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(oled_direct_test)
