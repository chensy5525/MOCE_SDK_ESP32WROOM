#include "mpu6050_direct_i2c.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "board.h"
#include "bsp_i2c.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_ACCEL_CONFIG     0x1CU
#define MPU6050_REG_ACCEL_XOUT_H     0x3BU
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_WHO_AM_I         0x75U

#define MPU6050_BURST_LEN            14U
#define MPU6050_ACCEL_LSB_PER_G      16384.0f
#define MPU6050_GYRO_LSB_PER_DPS     131.0f

static const char *TAG = "mpu6050_direct";

static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_address = MPU6050_DIRECT_ADDR_LOW;
static uint32_t s_timeout_ms = BOARD_I2C_TIMEOUT_MS;

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

void mpu6050_direct_default_config(mpu6050_direct_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->address = MPU6050_DIRECT_ADDR_LOW;
    config->i2c_speed_hz = MPU6050_DIRECT_DEFAULT_SPEED_HZ;
    config->timeout_ms = BOARD_I2C_TIMEOUT_MS;
}

uint8_t mpu6050_direct_scan(uint8_t *addresses, uint8_t max_addresses)
{
    uint8_t found = 0U;

    (void)bsp_i2c_init();

    for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
        if (bsp_i2c_probe(addr, 50) == ESP_OK) {
            if ((addresses != NULL) && (found < max_addresses)) {
                addresses[found] = addr;
            }
            ++found;
        }
    }

    return found;
}

esp_err_t mpu6050_direct_detect(uint8_t *address)
{
    static const uint8_t candidates[] = {
        MPU6050_DIRECT_ADDR_LOW,
        MPU6050_DIRECT_ADDR_HIGH,
    };

    ESP_RETURN_ON_FALSE(address != NULL, ESP_ERR_INVALID_ARG, TAG, "address is null");
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c init failed");

    for (size_t i = 0U; i < sizeof(candidates); ++i) {
        if (bsp_i2c_probe(candidates[i], BOARD_I2C_TIMEOUT_MS) == ESP_OK) {
            *address = candidates[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t mpu6050_direct_init(const mpu6050_direct_config_t *config)
{
    mpu6050_direct_config_t local_config;
    uint8_t who_am_i = 0U;

    if (config == NULL) {
        mpu6050_direct_default_config(&local_config);
        config = &local_config;
    }

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c init failed");

    if (s_dev != NULL) {
        ESP_RETURN_ON_ERROR(bsp_i2c_remove_device(s_dev), TAG, "remove old device failed");
        s_dev = NULL;
    }

    ESP_RETURN_ON_ERROR(
        bsp_i2c_probe(config->address, (int)config->timeout_ms),
        TAG, "MPU6050 probe failed at 0x%02X", config->address);
    ESP_RETURN_ON_ERROR(
        bsp_i2c_add_device_7bit(config->address, config->i2c_speed_hz, &s_dev),
        TAG, "add device failed");

    s_address = config->address;
    s_timeout_ms = config->timeout_ms;

    ESP_RETURN_ON_ERROR(
        bsp_i2c_read_reg_byte(s_dev, MPU6050_REG_WHO_AM_I, &who_am_i, (int)s_timeout_ms),
        TAG, "read WHO_AM_I failed");
    if (who_am_i != 0x68U) {
        ESP_LOGW(TAG, "unexpected WHO_AM_I=0x%02X, continue anyway", who_am_i);
    }

    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_PWR_MGMT_1, 0x00U, (int)s_timeout_ms),
        TAG, "wake MPU6050 failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_SMPLRT_DIV, 0x07U, (int)s_timeout_ms),
        TAG, "set sample divider failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_CONFIG, 0x03U, (int)s_timeout_ms),
        TAG, "set dlpf failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_GYRO_CONFIG, 0x00U, (int)s_timeout_ms),
        TAG, "set gyro range failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_ACCEL_CONFIG, 0x00U, (int)s_timeout_ms),
        TAG, "set accel range failed");

    ESP_LOGI(TAG, "MPU6050 initialized, addr=0x%02X WHO_AM_I=0x%02X", s_address, who_am_i);
    return ESP_OK;
}

esp_err_t mpu6050_direct_read_raw(mpu6050_direct_raw_t *raw)
{
    uint8_t data[MPU6050_BURST_LEN] = {0};

    ESP_RETURN_ON_FALSE(raw != NULL, ESP_ERR_INVALID_ARG, TAG, "raw is null");
    ESP_RETURN_ON_FALSE(s_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "device is not initialized");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_read_reg(s_dev, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data), (int)s_timeout_ms),
        TAG, "read sensor burst failed");

    raw->accel_x = read_i16_be(&data[0]);
    raw->accel_y = read_i16_be(&data[2]);
    raw->accel_z = read_i16_be(&data[4]);
    raw->temp = read_i16_be(&data[6]);
    raw->gyro_x = read_i16_be(&data[8]);
    raw->gyro_y = read_i16_be(&data[10]);
    raw->gyro_z = read_i16_be(&data[12]);

    return ESP_OK;
}

esp_err_t mpu6050_direct_read_sample(mpu6050_direct_sample_t *sample)
{
    mpu6050_direct_raw_t raw = {0};

    ESP_RETURN_ON_FALSE(sample != NULL, ESP_ERR_INVALID_ARG, TAG, "sample is null");
    ESP_RETURN_ON_ERROR(mpu6050_direct_read_raw(&raw), TAG, "read raw failed");

    memset(sample, 0, sizeof(*sample));
    sample->ax_g = (float)raw.accel_x / MPU6050_ACCEL_LSB_PER_G;
    sample->ay_g = (float)raw.accel_y / MPU6050_ACCEL_LSB_PER_G;
    sample->az_g = (float)raw.accel_z / MPU6050_ACCEL_LSB_PER_G;
    sample->ax_ms2 = sample->ax_g * MPU6050_DIRECT_GRAVITY_MS2;
    sample->ay_ms2 = sample->ay_g * MPU6050_DIRECT_GRAVITY_MS2;
    sample->az_ms2 = sample->az_g * MPU6050_DIRECT_GRAVITY_MS2;
    sample->gx_dps = (float)raw.gyro_x / MPU6050_GYRO_LSB_PER_DPS;
    sample->gy_dps = (float)raw.gyro_y / MPU6050_GYRO_LSB_PER_DPS;
    sample->gz_dps = (float)raw.gyro_z / MPU6050_GYRO_LSB_PER_DPS;
    sample->temp_c = ((float)raw.temp / 340.0f) + 36.53f;

    float accel_norm = sqrtf((sample->ax_g * sample->ax_g) +
                             (sample->ay_g * sample->ay_g) +
                             (sample->az_g * sample->az_g));
    if (accel_norm > 0.001f) {
        float cos_z = sample->az_g / accel_norm;
        if (cos_z > 1.0f) {
            cos_z = 1.0f;
        } else if (cos_z < -1.0f) {
            cos_z = -1.0f;
        }
        sample->tilt_deg = acosf(cos_z) * 57.2957795f;
    }

    return ESP_OK;
}
