/**
 * @file    mpu6050.h
 * @brief   MPU-6050 I2C 直连驱动公共接口
 */

#ifndef MPU6050_H__
#define MPU6050_H__

#include <stdbool.h>
#include <stdint.h>
#include "module_errors.h"

#define ERR_MPU6050_ID_MISMATCH             -10
#define ERR_MPU6050_CALIBRATION             -11
#define ERR_MPU6050_DATA_INVALID            -12

#define MPU6050_I2C_ADDR                    0x68U
#define MPU6050_I2C_FREQ_HZ                 400000U
#define MPU6050_I2C_TIMEOUT_MS              100U
#define MPU6050_SAMPLE_RATE_HZ              100U
/* Provisional tunables pending stationary-noise hardware validation. */
#define MPU6050_CALIBRATION_SAMPLES         50U
#define MPU6050_CALIBRATION_TIMEOUT_MS      2000U
#define MPU6050_COMPLEMENTARY_ALPHA_DEFAULT 0.98f

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;
    uint32_t timeout_ms;
    uint16_t calibration_samples;
    float complementary_alpha;
} mpu6050_cfg_t;

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    bool valid;
    bool accel_correction_used;
    uint32_t sample_count;
} mpu6050_orientation_t;

typedef struct mpu6050_ctx *mpu6050_handle_t;

int mpu6050_init_device(mpu6050_handle_t *handle, const mpu6050_cfg_t *cfg);
int mpu6050_deinit_device(mpu6050_handle_t handle);
int mpu6050_read_orientation(mpu6050_handle_t handle,
                             mpu6050_orientation_t *orientation);

#endif /* MPU6050_H__ */
