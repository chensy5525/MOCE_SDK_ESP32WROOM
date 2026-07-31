idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp_board esp_driver_gpio esp_driver_ledc
)
