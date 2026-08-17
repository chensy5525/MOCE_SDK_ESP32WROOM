idf_component_register(
    SRCS "bsp_i2s.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_gpio esp_driver_i2s freertos log
)
