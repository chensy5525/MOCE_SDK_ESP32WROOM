#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpu6050_direct_i2c.h"

#define MPU6050_SAMPLE_PERIOD_MS 500U

static const char *TAG = "mpu6050_direct_test";

static void print_sample(uint32_t sample_no, const mpu6050_direct_sample_t *sample)
{
    ESP_LOGI(TAG,
             "#%" PRIu32 " accel[g]=(%.3f,%.3f,%.3f) accel[m/s2]=(%.2f,%.2f,%.2f) "
             "gyro[dps]=(%.2f,%.2f,%.2f) temp=%.2fC tilt=%.1fdeg",
             sample_no,
             sample->ax_g, sample->ay_g, sample->az_g,
             sample->ax_ms2, sample->ay_ms2, sample->az_ms2,
             sample->gx_dps, sample->gy_dps, sample->gz_dps,
             sample->temp_c, sample->tilt_deg);

    printf("mpu6050_direct #%" PRIu32
           " ax=% .3fg ay=% .3fg az=% .3fg | "
           "gx=% .2fdps gy=% .2fdps gz=% .2fdps | "
           "temp=% .2fC tilt=% .1fdeg\r\n",
           sample_no,
           sample->ax_g, sample->ay_g, sample->az_g,
           sample->gx_dps, sample->gy_dps, sample->gz_dps,
           sample->temp_c, sample->tilt_deg);
}

static void print_scan_result(void)
{
    uint8_t addresses[16] = {0};
    uint8_t found = mpu6050_direct_scan(addresses, sizeof(addresses));

    printf("i2c scan done, found=%u", found);
    for (uint8_t i = 0U; (i < found) && (i < sizeof(addresses)); ++i) {
        printf(" 0x%02X", addresses[i]);
    }
    printf("\r\n");
}

void app_main(void)
{
    mpu6050_direct_config_t config;
    uint32_t sample_no = 0U;

    mpu6050_direct_default_config(&config);

    ESP_LOGI(TAG, "ESP32-WROOM mpu6050_direct_test");
    printf("\r\n==== ESP32-WROOM mpu6050_direct_test ====\r\n");
    printf("direct I2C, no CAN, no CH32\r\n");
    printf("I2C%d SDA=GPIO%d SCL=GPIO%d freq=%" PRIu32 "Hz addr=0x%02X/0x%02X\r\n\r\n",
           BOARD_I2C_PORT,
           BOARD_I2C_SDA_GPIO,
           BOARD_I2C_SCL_GPIO,
           config.i2c_speed_hz,
           MPU6050_DIRECT_ADDR_LOW,
           MPU6050_DIRECT_ADDR_HIGH);

    while (mpu6050_direct_detect(&config.address) != ESP_OK) {
        print_scan_result();
        ESP_LOGW(TAG, "MPU6050 not found at 0x%02X or 0x%02X, retrying in 1s",
                 MPU6050_DIRECT_ADDR_LOW, MPU6050_DIRECT_ADDR_HIGH);
        printf("mpu6050_direct not found, check SDA/SCL/VCC/GND\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (mpu6050_direct_init(&config) != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 init failed, retrying in 1s");
        printf("mpu6050_direct init failed, retrying\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) {
        mpu6050_direct_sample_t sample = {0};
        if (mpu6050_direct_read_sample(&sample) == ESP_OK) {
            ++sample_no;
            print_sample(sample_no, &sample);
        } else {
            ESP_LOGW(TAG, "MPU6050 sample read failed");
            printf("mpu6050_direct read failed\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(MPU6050_SAMPLE_PERIOD_MS));
    }
}
