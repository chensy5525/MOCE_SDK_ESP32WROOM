#include "mpu6050.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_EXAMPLE_TAG             "MPU6050_EXAMPLE"
#define MPU6050_CALCULATION_PERIOD_MS   10U
#define MPU6050_PRINT_PERIOD_MS         200U
#define MPU6050_PRINT_DIVIDER           \
    (MPU6050_PRINT_PERIOD_MS / MPU6050_CALCULATION_PERIOD_MS)
#define MPU6050_EXAMPLE_DURATION_MS     60000U

#define MPU6050_EXAMPLE_LOG_INF(format, ...) \
    printf("[INF][" MPU6050_EXAMPLE_TAG "] " format "\n", ##__VA_ARGS__)
#define MPU6050_EXAMPLE_LOG_WRN(format, ...) \
    printf("[WRN][" MPU6050_EXAMPLE_TAG "] " format "\n", ##__VA_ARGS__)
#define MPU6050_EXAMPLE_LOG_ERR(format, ...) \
    printf("[ERR][" MPU6050_EXAMPLE_TAG "] " format "\n", ##__VA_ARGS__)

void app_main(void)
{
    const mpu6050_cfg_t config = {
        .i2c_addr = MPU6050_I2C_ADDR,
        .clk_speed_hz = MPU6050_I2C_FREQ_HZ,
        .timeout_ms = MPU6050_I2C_TIMEOUT_MS,
        .calibration_samples = MPU6050_CALIBRATION_SAMPLES,
        .complementary_alpha = MPU6050_COMPLEMENTARY_ALPHA_DEFAULT,
    };
    mpu6050_orientation_t orientation;
    mpu6050_handle_t imu = NULL;
    TickType_t last_wake_time;
    TickType_t start_tick;
    const TickType_t calculation_period_ticks =
        pdMS_TO_TICKS(MPU6050_CALCULATION_PERIOD_MS);
    uint32_t calculation_count = 0U;
    uint32_t print_count = 0U;
    int result;

    MPU6050_EXAMPLE_LOG_INF("keep module stationary during startup calibration");
    result = mpu6050_init_device(&imu, &config);
    if (result != 0) {
        MPU6050_EXAMPLE_LOG_ERR("init FAIL, err=%d", result);
        return;
    }

    start_tick = xTaskGetTickCount();
    last_wake_time = start_tick;
    while ((TickType_t)(xTaskGetTickCount() - start_tick) <
           pdMS_TO_TICKS(MPU6050_EXAMPLE_DURATION_MS)) {
        result = mpu6050_read_orientation(imu, &orientation);
        if (result != 0) {
            MPU6050_EXAMPLE_LOG_WRN("sample FAIL, err=%d", result);
        } else {
            calculation_count++;
            if ((calculation_count % MPU6050_PRINT_DIVIDER) == 0U) {
                MPU6050_EXAMPLE_LOG_INF(
                    "roll=%.2fdeg pitch=%.2fdeg yaw=%.2fdeg valid=%u accel_fix=%u",
                    orientation.roll_deg, orientation.pitch_deg,
                    orientation.yaw_deg, orientation.valid ? 1U : 0U,
                    orientation.accel_correction_used ? 1U : 0U);
                print_count++;
            }
        }
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_wake_time) >= calculation_period_ticks) {
            last_wake_time = now;
        } else {
            vTaskDelayUntil(&last_wake_time, calculation_period_ticks);
        }
    }

    result = mpu6050_deinit_device(imu);
    if (result != 0) {
        MPU6050_EXAMPLE_LOG_ERR("deinit FAIL, err=%d", result);
    } else {
        MPU6050_EXAMPLE_LOG_INF("test complete, duration_ms=%u printed=%lu",
                                MPU6050_EXAMPLE_DURATION_MS,
                                (unsigned long)print_count);
    }
}
