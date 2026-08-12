idf_component_register(
    SRCS "ch32_i2c_multi_gateway_final.c"
    INCLUDE_DIRS "include"
    REQUIRES ch32_can_gateway_core esp_timer freertos
)
