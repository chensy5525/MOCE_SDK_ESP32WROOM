idf_component_register(
    SRCS "servo_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_gpio esp_driver_ledc
)
