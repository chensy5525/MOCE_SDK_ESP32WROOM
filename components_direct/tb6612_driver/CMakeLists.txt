idf_component_register(
    SRCS "tb6612_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_gpio bsp_pwm esp_driver_gpio esp_driver_ledc
)
