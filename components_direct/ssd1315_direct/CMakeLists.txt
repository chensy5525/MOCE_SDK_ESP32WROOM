idf_component_register(
    SRCS "ssd1315_direct.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c freertos
)
