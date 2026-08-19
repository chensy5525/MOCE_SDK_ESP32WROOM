cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.0")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_direct/max98357a_direct"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(max98357a_flash_mp3_test)
