cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.0")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_board"
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_uart"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_direct/e104_bt01_direct_uart"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(e104_bt01_direct_uart_test)
