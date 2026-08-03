#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_DIRECT_ADDR_LOW            0x68U
#define MPU6050_DIRECT_ADDR_HIGH           0x69U
#define MPU6050_DIRECT_DEFAULT_SPEED_HZ    100000U
#define MPU6050_DIRECT_GRAVITY_MS2         9.80665f

typedef struct {
    uint8_t address;
    uint32_t i2c_speed_hz;
    uint32_t timeout_ms;
} mpu6050_direct_config_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_direct_raw_t;

typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float ax_ms2;
    float ay_ms2;
    float az_ms2;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float temp_c;
    float tilt_deg;
} mpu6050_direct_sample_t;

void mpu6050_direct_default_config(mpu6050_direct_config_t *config);
esp_err_t mpu6050_direct_init(const mpu6050_direct_config_t *config);
esp_err_t mpu6050_direct_detect(uint8_t *address);
uint8_t mpu6050_direct_scan(uint8_t *addresses, uint8_t max_addresses);
esp_err_t mpu6050_direct_read_raw(mpu6050_direct_raw_t *raw);
esp_err_t mpu6050_direct_read_sample(mpu6050_direct_sample_t *sample);

#ifdef __cplusplus
}
#endif
