idf_component_register(
    SRCS "ch32_vl53l0x_gateway.c"
    INCLUDE_DIRS "include"
    REQUIRES ch32_i2c_multi_gateway_final freertos
)
