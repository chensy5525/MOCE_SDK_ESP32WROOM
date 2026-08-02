#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "bsp_i2c.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_I2C_ADDR             0x68U
#define MPU6050_I2C_ADDR_ALT         0x69U
#define MPU6050_I2C_SPEED_HZ         100000U

#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_ACCEL_CONFIG     0x1CU
#define MPU6050_REG_ACCEL_XOUT_H     0x3BU
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_WHO_AM_I         0x75U

#define MPU6050_BURST_LEN            14U
#define MPU6050_SAMPLE_PERIOD_MS     500U
#define MPU6050_ACCEL_LSB_PER_G      16384.0f
#define MPU6050_GYRO_LSB_PER_DPS     131.0f
#define MPU6050_GRAVITY_MS2          9.80665f

static const char *TAG = "mpu6050_direct_test";

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_raw_t;

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static esp_err_t mpu6050_read_raw(i2c_master_dev_handle_t dev, mpu6050_raw_t *raw)
{
    uint8_t data[MPU6050_BURST_LEN] = {0};

    ESP_RETURN_ON_FALSE(raw != NULL, ESP_ERR_INVALID_ARG, TAG, "raw is null");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_read_reg(dev, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data), BOARD_I2C_TIMEOUT_MS),
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

static uint8_t scan_i2c_bus(void)
{
    uint8_t found = 0U;

    printf("i2c scan start\r\n");
    for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
        esp_err_t err = bsp_i2c_probe(addr, 50);
        if (err == ESP_OK) {
            ++found;
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
            printf("i2c found addr=0x%02X\r\n", addr);
        }
    }

    printf("i2c scan done, found=%u\r\n", found);
    return found;
}

static esp_err_t detect_mpu6050_address(uint8_t *address)
{
    static const uint8_t candidates[] = {MPU6050_I2C_ADDR, MPU6050_I2C_ADDR_ALT};

    ESP_RETURN_ON_FALSE(address != NULL, ESP_ERR_INVALID_ARG, TAG, "address is null");

    for (size_t i = 0U; i < sizeof(candidates); ++i) {
        esp_err_t err = bsp_i2c_probe(candidates[i], BOARD_I2C_TIMEOUT_MS);
        if (err == ESP_OK) {
            *address = candidates[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t mpu6050_init(i2c_master_dev_handle_t dev, uint8_t address)
{
    uint8_t who_am_i = 0U;

    ESP_RETURN_ON_ERROR(
        bsp_i2c_probe(address, BOARD_I2C_TIMEOUT_MS),
        TAG, "MPU6050 probe failed at 0x%02X", address);

    ESP_RETURN_ON_ERROR(
        bsp_i2c_read_reg_byte(dev, MPU6050_REG_WHO_AM_I, &who_am_i, BOARD_I2C_TIMEOUT_MS),
        TAG, "read WHO_AM_I failed");

    if (who_am_i != 0x68U) {
        ESP_LOGW(TAG, "unexpected WHO_AM_I=0x%02X, continue anyway", who_am_i);
    }

    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(dev, MPU6050_REG_PWR_MGMT_1, 0x00U, BOARD_I2C_TIMEOUT_MS),
        TAG, "wake MPU6050 failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(dev, MPU6050_REG_SMPLRT_DIV, 0x07U, BOARD_I2C_TIMEOUT_MS),
        TAG, "set sample divider failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(dev, MPU6050_REG_CONFIG, 0x03U, BOARD_I2C_TIMEOUT_MS),
        TAG, "set dlpf failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(dev, MPU6050_REG_GYRO_CONFIG, 0x00U, BOARD_I2C_TIMEOUT_MS),
        TAG, "set gyro range failed");
    ESP_RETURN_ON_ERROR(
        bsp_i2c_write_reg_byte(dev, MPU6050_REG_ACCEL_CONFIG, 0x00U, BOARD_I2C_TIMEOUT_MS),
        TAG, "set accel range failed");

    ESP_LOGI(TAG, "MPU6050 initialized, addr=0x%02X WHO_AM_I=0x%02X",
             address, who_am_i);
    return ESP_OK;
}

static void print_sample(uint32_t sample_no, const mpu6050_raw_t *raw)
{
    float ax_g = (float)raw->accel_x / MPU6050_ACCEL_LSB_PER_G;
    float ay_g = (float)raw->accel_y / MPU6050_ACCEL_LSB_PER_G;
    float az_g = (float)raw->accel_z / MPU6050_ACCEL_LSB_PER_G;
    float gx_dps = (float)raw->gyro_x / MPU6050_GYRO_LSB_PER_DPS;
    float gy_dps = (float)raw->gyro_y / MPU6050_GYRO_LSB_PER_DPS;
    float gz_dps = (float)raw->gyro_z / MPU6050_GYRO_LSB_PER_DPS;
    float temp_c = ((float)raw->temp / 340.0f) + 36.53f;

    float accel_norm = sqrtf((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    float tilt_deg = 0.0f;
    if (accel_norm > 0.001f) {
        float cos_z = az_g / accel_norm;
        if (cos_z > 1.0f) {
            cos_z = 1.0f;
        } else if (cos_z < -1.0f) {
            cos_z = -1.0f;
        }
        tilt_deg = acosf(cos_z) * 57.2957795f;
    }

    ESP_LOGI(TAG,
             "#%" PRIu32 " accel[g]=(%.3f,%.3f,%.3f) accel[m/s2]=(%.2f,%.2f,%.2f) "
             "gyro[dps]=(%.2f,%.2f,%.2f) temp=%.2fC tilt=%.1fdeg",
             sample_no,
             ax_g, ay_g, az_g,
             ax_g * MPU6050_GRAVITY_MS2,
             ay_g * MPU6050_GRAVITY_MS2,
             az_g * MPU6050_GRAVITY_MS2,
             gx_dps, gy_dps, gz_dps,
             temp_c, tilt_deg);

    printf("mpu6050_direct #%" PRIu32
           " ax=% .3fg ay=% .3fg az=% .3fg | "
           "gx=% .2fdps gy=% .2fdps gz=% .2fdps | "
           "temp=% .2fC tilt=% .1fdeg\r\n",
           sample_no,
           ax_g, ay_g, az_g,
           gx_dps, gy_dps, gz_dps,
           temp_c, tilt_deg);
}

void app_main(void)
{
    i2c_master_dev_handle_t mpu_dev = NULL;
    uint8_t mpu_addr = MPU6050_I2C_ADDR;
    uint32_t sample_no = 0U;

    ESP_LOGI(TAG, "ESP32-WROOM mpu6050_direct_test");
    printf("\r\n==== ESP32-WROOM mpu6050_direct_test ====\r\n");
    printf("direct I2C, no CAN, no CH32\r\n");
    printf("I2C%d SDA=GPIO%d SCL=GPIO%d freq=%uHz addr=0x%02X/0x%02X\r\n\r\n",
           BOARD_I2C_PORT,
           BOARD_I2C_SDA_GPIO,
           BOARD_I2C_SCL_GPIO,
           MPU6050_I2C_SPEED_HZ,
           MPU6050_I2C_ADDR,
           MPU6050_I2C_ADDR_ALT);

    ESP_ERROR_CHECK(bsp_i2c_init());

    while (detect_mpu6050_address(&mpu_addr) != ESP_OK) {
        scan_i2c_bus();
        ESP_LOGW(TAG, "MPU6050 not found at 0x%02X or 0x%02X, retrying in 1s",
                 MPU6050_I2C_ADDR, MPU6050_I2C_ADDR_ALT);
        printf("mpu6050_direct not found, check SDA/SCL/VCC/GND\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(bsp_i2c_add_device_7bit(mpu_addr, MPU6050_I2C_SPEED_HZ, &mpu_dev));

    while (mpu6050_init(mpu_dev, mpu_addr) != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 init failed, retrying in 1s");
        printf("mpu6050_direct init failed, retrying\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) {
        mpu6050_raw_t raw = {0};
        if (mpu6050_read_raw(mpu_dev, &raw) == ESP_OK) {
            ++sample_no;
            print_sample(sample_no, &raw);
        } else {
            ESP_LOGW(TAG, "MPU6050 sample read failed");
            printf("mpu6050_direct read failed\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(MPU6050_SAMPLE_PERIOD_MS));
    }
}
