#include "vl53l4cd.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L4CD_EXAMPLE"
#define VL53L4CD_EXAMPLE_DURATION_MS 60000U
#define VL53L4CD_EXAMPLE_SAMPLE_PERIOD_MS 200U
#define VL53L4CD_EXAMPLE_PRINT_EVERY 5U
void app_main(void)
{
    const vl53l4cd_cfg_t config = {
        .i2c_addr = VL53L4CD_I2C_ADDR_DEFAULT,
        .bus_speed_hz = VL53L4CD_BUS_SPEED_HZ,
        .timeout_ms = VL53L4CD_I2C_TIMEOUT_MS,
        .data_ready_timeout_ms = VL53L4CD_DATA_READY_TIMEOUT_MS,
    };
    vl53l4cd_handle_t sensor = NULL;
    vl53l4cd_result_t measurement;
    TickType_t start_tick;
    TickType_t last_wake;
    uint32_t sample_count = 0U;
    int result = vl53l4cd_init(&sensor, &config);

    if (result != 0) {
        printf("[ERR][" TAG "] init FAIL err=%d\n", result);
        return;
    }

    start_tick = xTaskGetTickCount();
    last_wake = start_tick;
    while ((TickType_t)(xTaskGetTickCount() - start_tick) <
           pdMS_TO_TICKS(VL53L4CD_EXAMPLE_DURATION_MS)) {
        result = vl53l4cd_read(sensor, &measurement);
        ++sample_count;
        if (result == 0) {
            if (sample_count % VL53L4CD_EXAMPLE_PRINT_EVERY == 0U) {
                printf("[INF][" TAG "] data valid=1 mm=%u status=%u sample=%u\n",
                       measurement.distance_mm, measurement.range_status,
                       (unsigned)sample_count);
            }
        } else if (result == ERR_VL53L4CD_OUT_OF_RANGE) {
            printf("[WRN][" TAG "] OUT RANGE mm=%u status=%u\n",
                   measurement.distance_mm, measurement.range_status);
        } else {
            printf("[ERR][" TAG "] read FAIL err=%d\n", result);
        }
        if ((TickType_t)(xTaskGetTickCount() - start_tick) >=
            pdMS_TO_TICKS(VL53L4CD_EXAMPLE_DURATION_MS)) {
            break;
        }
        vTaskDelayUntil(&last_wake,
                        pdMS_TO_TICKS(VL53L4CD_EXAMPLE_SAMPLE_PERIOD_MS));
    }

    result = vl53l4cd_deinit(sensor);
    if (result != 0) {
        printf("[ERR][" TAG "] deinit FAIL err=%d\n", result);
    }
    printf("[INF][" TAG "] example complete duration_ms=%u samples=%u\n",
           VL53L4CD_EXAMPLE_DURATION_MS, (unsigned)sample_count);
}
