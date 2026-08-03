cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.0")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_ch32/ch32_vc02_gateway"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vc02_ch32_test)
