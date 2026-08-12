idf_component_register(
    SRCS "ch32_i2c_multi_gateway_final.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board driver esp_timer freertos
)
