cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_board"
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_uart"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_direct/syn6288_direct"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(syn6288_direct_test0_final)
