/**
 * @file mpu6050.h
 * @brief MPU-6050 CH32-CAN-I2C bridge driver public API
 */
#ifndef CH32_MPU6050_H__
#define CH32_MPU6050_H__

#include <stdbool.h>
#include <stdint.h>
#include "ch32_i2c_multi_gateway_final.h"

#ifndef ERR_TIMEOUT
#define ERR_TIMEOUT -1
#define ERR_BUSY -2
#define ERR_NOT_INIT -3
#define ERR_INVALID_PARAM -4
#define ERR_NOT_SUPPORTED -5
#define ERR_OVERFLOW -6
#define ERR_NO_DEVICE -7
#define ERR_HW_FAULT -8
#endif

#define ERR_MPU6050_ID_MISMATCH -10
#define ERR_MPU6050_CALIBRATION -11
#define ERR_MPU6050_DATA_INVALID -12
#define MPU6050_I2C_ADDR 0x68U
#define MPU6050_I2C_FREQ_HZ 400000U
#define MPU6050_BRIDGE_TIMEOUT_MS 2000U
#define MPU6050_SAMPLE_RATE_HZ 100U
#define MPU6050_CALIBRATION_SAMPLES 200U
#define MPU6050_COMPLEMENTARY_ALPHA_DEFAULT 0.98f
#define MPU6050_MAX_BRIDGE_INSTANCES 6U

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;
    uint32_t bridge_timeout_ms;
    uint16_t calibration_samples;
    float complementary_alpha;
    /* Discovery owns this stable record and updates it in place by token. */
    ch32_i2c_multi_node_t *ch32_node;
} mpu6050_cfg_t;

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    bool valid;
    uint32_t sample_count;
} mpu6050_orientation_t;

typedef struct mpu6050_ctx *mpu6050_handle_t;
int mpu6050_init_device(mpu6050_handle_t *handle, const mpu6050_cfg_t *cfg);
int mpu6050_deinit_device(mpu6050_handle_t handle);
int mpu6050_read_orientation(mpu6050_handle_t handle,
                             mpu6050_orientation_t *orientation);
#endif
