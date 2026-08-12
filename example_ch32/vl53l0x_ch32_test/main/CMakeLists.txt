idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ch32_i2c_multi_gateway_final ch32_vl53l0x_gateway freertos
)
