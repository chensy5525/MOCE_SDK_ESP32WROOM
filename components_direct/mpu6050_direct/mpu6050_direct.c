/**
 * @file    mpu6050_direct.c
 * @brief   MPU-6050 I2C 直连姿态驱动
 * @note    寄存器定义来自 InvenSense RM-MPU-6000A-00 Rev.4.2。
 */

#include "mpu6050.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bsp_i2c.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_TAG                   "MPU6050"
#define MPU6050_MAX_RETRIES           3U
#define MPU6050_RETRY_DELAY_MS        2U
#define MPU6050_RESET_DELAY_MS        100U
#define MPU6050_SAMPLE_DELAY_MS       (1000U / MPU6050_SAMPLE_RATE_HZ)
#define MPU6050_SAMPLE_BURST_LEN      14U
#define MPU6050_WHO_AM_I_VALUE        0x68U

#define MPU6050_REG_SMPLRT_DIV        0x19U
#define MPU6050_REG_CONFIG            0x1AU
#define MPU6050_REG_GYRO_CONFIG       0x1BU
#define MPU6050_REG_ACCEL_CONFIG      0x1CU
#define MPU6050_REG_ACCEL_XOUT_H      0x3BU
#define MPU6050_REG_PWR_MGMT_1        0x6BU
#define MPU6050_REG_PWR_MGMT_2        0x6CU
#define MPU6050_REG_WHO_AM_I          0x75U

#define MPU6050_PWR_DEVICE_RESET      0x80U
#define MPU6050_CLOCK_PLL_X_GYRO      0x01U
#define MPU6050_ALL_AXES_ENABLED      0x00U
#define MPU6050_DLPF_CFG_3            0x03U
#define MPU6050_SMPLRT_DIV_100_HZ     9U
#define MPU6050_GYRO_FS_250_DPS       0x00U
#define MPU6050_ACCEL_FS_2_G          0x00U

#define MPU6050_ACCEL_LSB_PER_G       16384.0f
#define MPU6050_GYRO_LSB_PER_DPS      131.0f
#define MPU6050_RAD_TO_DEG            57.29577951308232f
#define MPU6050_ACCEL_NORM_MIN_G      0.5f
#define MPU6050_ACCEL_NORM_MAX_G      1.5f
#define MPU6050_MAX_DELTA_TIME_S      0.1f
#define MPU6050_MIN_CALIBRATION_COUNT 20U

#define MPU6050_LOG_INF(format, ...) \
    printf("[INF][" MPU6050_TAG "] " format "\n", ##__VA_ARGS__)
#define MPU6050_LOG_WRN(format, ...) \
    printf("[WRN][" MPU6050_TAG "] " format "\n", ##__VA_ARGS__)
#define MPU6050_LOG_ERR(format, ...) \
    printf("[ERR][" MPU6050_TAG "] " format "\n", ##__VA_ARGS__)

typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
} mpu6050_sample_t;

typedef struct mpu6050_ctx {
    i2c_master_dev_handle_t i2c_device;
    uint8_t i2c_addr;
    uint32_t timeout_ms;
    float complementary_alpha;
    float gyro_bias_x_dps;
    float gyro_bias_y_dps;
    float gyro_bias_z_dps;
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    int64_t last_update_us;
    uint32_t sample_count;
    uint32_t ok_count;
    uint32_t error_count;
    bool initialized;
    bool filter_seeded;
} mpu6050_ctx_t;

static mpu6050_ctx_t s_ctx;
static bool s_ctx_in_use;

static int mpu6050_map_esp_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return 0;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return ERR_TIMEOUT;
    }
    if (error == ESP_ERR_INVALID_ARG || error == ESP_ERR_INVALID_SIZE) {
        return ERR_INVALID_PARAM;
    }
    if (error == ESP_ERR_INVALID_STATE) {
        return ERR_NOT_INIT;
    }
    return ERR_NO_DEVICE;
}

static int16_t mpu6050_decode_i16(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
}

static float mpu6050_wrap_angle(float angle_deg)
{
    while (angle_deg >= 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static int mpu6050_write_register(mpu6050_handle_t handle, uint8_t reg,
                                  uint8_t value)
{
    esp_err_t error = ESP_FAIL;

    for (uint32_t attempt = 0U; attempt < MPU6050_MAX_RETRIES; ++attempt) {
        error = bsp_i2c_write_reg_byte(handle->i2c_device, reg, value,
                                       (int)handle->timeout_ms);
        if (error == ESP_OK) {
            handle->ok_count++;
            return 0;
        }
        handle->error_count++;
        if (attempt + 1U < MPU6050_MAX_RETRIES) {
            MPU6050_LOG_WRN("i2c write retry=%lu reg=0x%02X",
                            (unsigned long)(attempt + 1U), reg);
            vTaskDelay(pdMS_TO_TICKS(MPU6050_RETRY_DELAY_MS));
        }
    }
    return mpu6050_map_esp_error(error);
}

static int mpu6050_read_registers(mpu6050_handle_t handle, uint8_t reg,
                                  uint8_t *data, size_t len)
{
    esp_err_t error = ESP_FAIL;

    if (data == NULL || len == 0U) {
        return ERR_INVALID_PARAM;
    }
    for (uint32_t attempt = 0U; attempt < MPU6050_MAX_RETRIES; ++attempt) {
        error = bsp_i2c_read_reg(handle->i2c_device, reg, data, len,
                                 (int)handle->timeout_ms);
        if (error == ESP_OK) {
            handle->ok_count++;
            return 0;
        }
        handle->error_count++;
        if (attempt + 1U < MPU6050_MAX_RETRIES) {
            MPU6050_LOG_WRN("i2c read retry=%lu reg=0x%02X",
                            (unsigned long)(attempt + 1U), reg);
            vTaskDelay(pdMS_TO_TICKS(MPU6050_RETRY_DELAY_MS));
        }
    }
    return mpu6050_map_esp_error(error);
}

static int mpu6050_read_sample(mpu6050_handle_t handle,
                               mpu6050_sample_t *sample)
{
    uint8_t data[MPU6050_SAMPLE_BURST_LEN];
    int result;

    result = mpu6050_read_registers(handle, MPU6050_REG_ACCEL_XOUT_H,
                                    data, sizeof(data));
    if (result != 0) {
        return result;
    }

    sample->ax_g = (float)mpu6050_decode_i16(&data[0]) / MPU6050_ACCEL_LSB_PER_G;
    sample->ay_g = (float)mpu6050_decode_i16(&data[2]) / MPU6050_ACCEL_LSB_PER_G;
    sample->az_g = (float)mpu6050_decode_i16(&data[4]) / MPU6050_ACCEL_LSB_PER_G;
    sample->gx_dps = (float)mpu6050_decode_i16(&data[8]) / MPU6050_GYRO_LSB_PER_DPS;
    sample->gy_dps = (float)mpu6050_decode_i16(&data[10]) / MPU6050_GYRO_LSB_PER_DPS;
    sample->gz_dps = (float)mpu6050_decode_i16(&data[12]) / MPU6050_GYRO_LSB_PER_DPS;
    return 0;
}

static int mpu6050_configure_device(mpu6050_handle_t handle)
{
    uint8_t who_am_i = 0U;
    int result;

    result = mpu6050_write_register(handle, MPU6050_REG_PWR_MGMT_1,
                                    MPU6050_PWR_DEVICE_RESET);
    if (result != 0) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(MPU6050_RESET_DELAY_MS));

    result = mpu6050_read_registers(handle, MPU6050_REG_WHO_AM_I,
                                    &who_am_i, sizeof(who_am_i));
    if (result != 0) {
        return result;
    }
    if (who_am_i != MPU6050_WHO_AM_I_VALUE) {
        MPU6050_LOG_ERR("init FAIL, reason=id_mismatch expected=0x%02X got=0x%02X",
                        MPU6050_WHO_AM_I_VALUE, who_am_i);
        return ERR_MPU6050_ID_MISMATCH;
    }

    result = mpu6050_write_register(handle, MPU6050_REG_PWR_MGMT_1,
                                    MPU6050_CLOCK_PLL_X_GYRO);
    if (result == 0) {
        result = mpu6050_write_register(handle, MPU6050_REG_PWR_MGMT_2,
                                        MPU6050_ALL_AXES_ENABLED);
    }
    if (result == 0) {
        result = mpu6050_write_register(handle, MPU6050_REG_CONFIG,
                                        MPU6050_DLPF_CFG_3);
    }
    if (result == 0) {
        result = mpu6050_write_register(handle, MPU6050_REG_SMPLRT_DIV,
                                        MPU6050_SMPLRT_DIV_100_HZ);
    }
    if (result == 0) {
        result = mpu6050_write_register(handle, MPU6050_REG_GYRO_CONFIG,
                                        MPU6050_GYRO_FS_250_DPS);
    }
    if (result == 0) {
        result = mpu6050_write_register(handle, MPU6050_REG_ACCEL_CONFIG,
                                        MPU6050_ACCEL_FS_2_G);
    }
    return result;
}

static int mpu6050_calibrate_gyro(mpu6050_handle_t handle,
                                  uint16_t calibration_samples)
{
    mpu6050_sample_t sample;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    uint16_t valid_count = 0U;

    for (uint16_t index = 0U; index < calibration_samples; ++index) {
        if (mpu6050_read_sample(handle, &sample) == 0) {
            sum_x += sample.gx_dps;
            sum_y += sample.gy_dps;
            sum_z += sample.gz_dps;
            valid_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(MPU6050_SAMPLE_DELAY_MS));
    }
    if (valid_count < MPU6050_MIN_CALIBRATION_COUNT ||
        valid_count < (uint16_t)(calibration_samples / 2U)) {
        return ERR_MPU6050_CALIBRATION;
    }

    handle->gyro_bias_x_dps = sum_x / (float)valid_count;
    handle->gyro_bias_y_dps = sum_y / (float)valid_count;
    handle->gyro_bias_z_dps = sum_z / (float)valid_count;
    MPU6050_LOG_INF("calibration OK, samples=%u", valid_count);
    return 0;
}

int mpu6050_init_device(mpu6050_handle_t *handle, const mpu6050_cfg_t *cfg)
{
    esp_err_t error;
    int result;

    if (handle == NULL || cfg == NULL) {
        MPU6050_LOG_ERR("init FAIL, reason=invalid_param");
        return ERR_INVALID_PARAM;
    }
    *handle = NULL;
    if (cfg->i2c_addr != MPU6050_I2C_ADDR ||
        cfg->clk_speed_hz == 0U || cfg->clk_speed_hz > MPU6050_I2C_FREQ_HZ ||
        cfg->timeout_ms == 0U ||
        cfg->calibration_samples < MPU6050_MIN_CALIBRATION_COUNT ||
        cfg->complementary_alpha < 0.0f || cfg->complementary_alpha > 1.0f) {
        MPU6050_LOG_ERR("init FAIL, reason=invalid_config");
        return ERR_INVALID_PARAM;
    }
    if (s_ctx_in_use) {
        MPU6050_LOG_ERR("init FAIL, reason=busy");
        return ERR_BUSY;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.i2c_addr = cfg->i2c_addr;
    s_ctx.timeout_ms = cfg->timeout_ms;
    s_ctx.complementary_alpha = cfg->complementary_alpha;
    s_ctx_in_use = true;

    if (cfg->initialize_i2c || !bsp_i2c_is_initialized()) {
        error = bsp_i2c_init();
        if (error != ESP_OK) {
            result = mpu6050_map_esp_error(error);
            MPU6050_LOG_ERR("init FAIL, reason=i2c_bus err=%d", result);
            goto fail;
        }
    }
    error = bsp_i2c_add_device_7bit(cfg->i2c_addr, cfg->clk_speed_hz,
                                    &s_ctx.i2c_device);
    if (error != ESP_OK) {
        result = mpu6050_map_esp_error(error);
        MPU6050_LOG_ERR("init FAIL, reason=i2c_device err=%d", result);
        goto fail;
    }

    result = mpu6050_configure_device(&s_ctx);
    if (result != 0) {
        MPU6050_LOG_ERR("init FAIL, reason=device_config err=%d", result);
        goto fail_device;
    }
    result = mpu6050_calibrate_gyro(&s_ctx, cfg->calibration_samples);
    if (result != 0) {
        MPU6050_LOG_ERR("init FAIL, reason=calibration err=%d", result);
        goto fail_device;
    }

    s_ctx.initialized = true;
    s_ctx.last_update_us = esp_timer_get_time();
    *handle = &s_ctx;
    MPU6050_LOG_INF("init OK, addr=0x%02X rate=%uHz accel=+/-2g gyro=+/-250dps",
                    s_ctx.i2c_addr, MPU6050_SAMPLE_RATE_HZ);
    return 0;

fail_device:
    (void)bsp_i2c_remove_device(s_ctx.i2c_device);
fail:
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx_in_use = false;
    return result;
}

int mpu6050_deinit_device(mpu6050_handle_t handle)
{
    if (handle == NULL || handle != &s_ctx || !handle->initialized) {
        return ERR_NOT_INIT;
    }

    handle->initialized = false;
    (void)bsp_i2c_remove_device(handle->i2c_device);
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx_in_use = false;
    MPU6050_LOG_INF("deinit OK");
    return 0;
}

int mpu6050_read_orientation(mpu6050_handle_t handle,
                             mpu6050_orientation_t *orientation)
{
    mpu6050_sample_t sample;
    float accel_roll_deg;
    float accel_pitch_deg;
    float accel_norm_g;
    float delta_time_s;
    float gyro_roll_deg;
    float gyro_pitch_deg;
    int64_t now_us;
    int result;

    if (handle == NULL || handle != &s_ctx || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    if (orientation == NULL) {
        return ERR_INVALID_PARAM;
    }
    memset(orientation, 0, sizeof(*orientation));

    result = mpu6050_read_sample(handle, &sample);
    if (result != 0) {
        MPU6050_LOG_ERR("read FAIL, stage=i2c err=%d", result);
        return result;
    }

    accel_norm_g = sqrtf(sample.ax_g * sample.ax_g + sample.ay_g * sample.ay_g +
                         sample.az_g * sample.az_g);
    if (!isfinite(accel_norm_g) || accel_norm_g < MPU6050_ACCEL_NORM_MIN_G ||
        accel_norm_g > MPU6050_ACCEL_NORM_MAX_G) {
        handle->error_count++;
        MPU6050_LOG_WRN("data invalid, reason=accel_norm");
        return ERR_MPU6050_DATA_INVALID;
    }

    accel_roll_deg = atan2f(sample.ay_g, sample.az_g) * MPU6050_RAD_TO_DEG;
    accel_pitch_deg = atan2f(-sample.ax_g,
                            sqrtf(sample.ay_g * sample.ay_g +
                                  sample.az_g * sample.az_g)) * MPU6050_RAD_TO_DEG;
    now_us = esp_timer_get_time();
    delta_time_s = (float)(now_us - handle->last_update_us) / 1000000.0f;
    handle->last_update_us = now_us;

    if (!handle->filter_seeded || delta_time_s <= 0.0f ||
        delta_time_s > MPU6050_MAX_DELTA_TIME_S) {
        handle->roll_deg = accel_roll_deg;
        handle->pitch_deg = accel_pitch_deg;
        handle->filter_seeded = true;
    } else {
        gyro_roll_deg = handle->roll_deg +
                        (sample.gx_dps - handle->gyro_bias_x_dps) * delta_time_s;
        gyro_pitch_deg = handle->pitch_deg +
                         (sample.gy_dps - handle->gyro_bias_y_dps) * delta_time_s;
        handle->roll_deg = handle->complementary_alpha * gyro_roll_deg +
                           (1.0f - handle->complementary_alpha) * accel_roll_deg;
        handle->pitch_deg = handle->complementary_alpha * gyro_pitch_deg +
                            (1.0f - handle->complementary_alpha) * accel_pitch_deg;
        handle->yaw_deg = mpu6050_wrap_angle(
            handle->yaw_deg +
            (sample.gz_dps - handle->gyro_bias_z_dps) * delta_time_s);
    }

    handle->sample_count++;
    orientation->roll_deg = mpu6050_wrap_angle(handle->roll_deg);
    orientation->pitch_deg = mpu6050_wrap_angle(handle->pitch_deg);
    orientation->yaw_deg = handle->yaw_deg;
    orientation->valid = true;
    orientation->sample_count = handle->sample_count;
    return 0;
}
