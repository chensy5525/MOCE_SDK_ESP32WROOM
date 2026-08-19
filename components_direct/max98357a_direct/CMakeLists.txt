idf_component_register(
    SRCS "max98357a_direct.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_i2s esp_common
)
