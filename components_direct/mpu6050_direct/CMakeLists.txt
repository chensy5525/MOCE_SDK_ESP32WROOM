idf_component_register(
    SRCS "mpu6050_direct.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c freertos esp_timer
)
