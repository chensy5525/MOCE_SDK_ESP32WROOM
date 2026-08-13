cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "1.0.0")
set(EXTRA_COMPONENT_DIRS
 "${CMAKE_CURRENT_LIST_DIR}/../../bsp"
 "${CMAKE_CURRENT_LIST_DIR}/../../components_esp32wroom/ch32_can_gateway_core"
 "${CMAKE_CURRENT_LIST_DIR}/../../components_esp32wroom/ch32_i2c_multi_gateway_final"
 "${CMAKE_CURRENT_LIST_DIR}/../../components_ch32/ch32_vl53l4cd_gateway")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vl53l4cd_ch32_test)
