idf_component_register(
    SRCS "ch32_ssd1315_gateway.c"
    INCLUDE_DIRS "include"
    REQUIRES ch32_i2c_multi_gateway_final freertos module_errors
)
