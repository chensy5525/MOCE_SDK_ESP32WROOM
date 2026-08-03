idf_component_register(
    SRCS "vl53l0x_direct_i2c.c" "vl53l0x_direct_policy.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES bsp_i2c esp_driver_gpio
)
