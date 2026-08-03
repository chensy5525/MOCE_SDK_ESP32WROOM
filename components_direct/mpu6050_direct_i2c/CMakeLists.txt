idf_component_register(
    SRCS "mpu6050_direct_i2c.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c
)
