/**
 * @file ch32_mpu6050_gateway.h
 * @brief MPU-6050 CH32-CAN-I2C bridge driver public API
 */
#ifndef CH32_MPU6050_GATEWAY_H__
#define CH32_MPU6050_GATEWAY_H__

#include <stdbool.h>
#include <stdint.h>
#include "ch32_i2c_multi_gateway_final.h"
#include "module_errors.h"

#define ERR_CH32_MPU6050_ID_MISMATCH -10
#define ERR_CH32_MPU6050_CALIBRATION -11
#define ERR_CH32_MPU6050_DATA_INVALID -12
#define CH32_MPU6050_I2C_ADDR 0x68U
#define CH32_MPU6050_I2C_FREQ_HZ 400000U
#define CH32_MPU6050_GATEWAY_TIMEOUT_MS 350U
#define CH32_MPU6050_READ_TIMEOUT_MS 200U
#define CH32_MPU6050_CALIBRATION_TIMEOUT_MS 3000U
#define CH32_MPU6050_SAMPLE_MAX_ATTEMPTS 2U
#define CH32_MPU6050_SAMPLE_RATE_HZ 100U
#define CH32_MPU6050_CALIBRATION_SAMPLES 50U
#define CH32_MPU6050_COMPLEMENTARY_ALPHA_DEFAULT 0.98f
#define CH32_MPU6050_MAX_INSTANCES 6U

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;
    uint16_t calibration_samples;
    float complementary_alpha;
    ch32_i2c_multi_node_t *ch32_node;
} ch32_mpu6050_cfg_t;

typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    bool valid;
    bool accel_correction_used;
    uint32_t sample_count;
} ch32_mpu6050_orientation_t;

typedef struct ch32_mpu6050_ctx *ch32_mpu6050_handle_t;

int ch32_mpu6050_init(ch32_mpu6050_handle_t *handle,
                      const ch32_mpu6050_cfg_t *cfg);
int ch32_mpu6050_deinit(ch32_mpu6050_handle_t handle);
int ch32_mpu6050_read_orientation(
    ch32_mpu6050_handle_t handle,
    ch32_mpu6050_orientation_t *orientation);

#endif
