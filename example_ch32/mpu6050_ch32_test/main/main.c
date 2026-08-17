#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ch32_i2c_multi_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ch32_mpu6050_gateway.h"

#define TAG "MPU6050_EXAMPLE"
#define LOGI(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGW(f, ...) printf("[WRN][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGE(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define MAX_NODES 6U
#define INITIAL_DISCOVERY_MS 1000U
#define REDISCOVERY_TIMEOUT_MS 1000U
#define REDISCOVERY_MISSING_MS 10000U
#define REDISCOVERY_FAST_FIRST_MS 250U
#define REDISCOVERY_FAST_MAX_MS 2000U
#define CALCULATION_MS 10U
#define PRINT_DIVIDER 20U
#define EXAMPLE_DURATION_MS 60000U

static ch32_i2c_multi_node_t s_nodes[MAX_NODES];
static size_t s_node_count;
static ch32_i2c_multi_node_t *s_active_node;
static ch32_mpu6050_handle_t s_imu;

static bool try_bind_first_mpu6050(void)
{
    for (size_t i = 0U; i < s_node_count; ++i) {
        ch32_i2c_multi_node_t *node = &s_nodes[i];
        const ch32_mpu6050_cfg_t cfg = {
            .i2c_addr = CH32_MPU6050_I2C_ADDR,
            .clk_speed_hz = CH32_MPU6050_I2C_FREQ_HZ,
            .calibration_samples = CH32_MPU6050_CALIBRATION_SAMPLES,
            .complementary_alpha = CH32_MPU6050_COMPLEMENTARY_ALPHA_DEFAULT,
            .ch32_node = node,
        };
        int result;

        if (!node->ready ||
            node->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C) {
            continue;
        }
        LOGI("module calibration start token=0x%04X; keep stationary",
             node->token);
        result = ch32_mpu6050_init(&s_imu, &cfg);
        if (result == 0) {
            s_active_node = node;
            LOGI("module ready token=0x%04X node=%u",
                 node->token, node->node_id);
            return true;
        }
        LOGW("candidate rejected token=0x%04X node=%u err=%d",
             node->token, node->node_id, result);
    }
    return false;
}

static void discover_missing_module(uint32_t timeout_ms)
{
    ch32_i2c_multi_result_t result = ch32_i2c_multi_discover_incremental(
        s_nodes, MAX_NODES, &s_node_count, timeout_ms);

    LOGI("F0/F1/F2 discovery result=%s candidates=%u",
         ch32_i2c_multi_result_text(result), (unsigned)s_node_count);
    if (s_imu == NULL && !try_bind_first_mpu6050()) {
        LOGW("MPU6050 not found; retry later");
    }
}

void app_main(void)
{
    ch32_i2c_multi_config_t cfg;
    uint32_t calculations = 0U;
    uint32_t prints = 0U;
    uint32_t consecutive_failures = 0U;
    uint32_t retry_delay_ms = REDISCOVERY_FAST_FIRST_MS;
    TickType_t start_tick;
    TickType_t last_wake;

    ch32_i2c_multi_default_config(&cfg);
    cfg.discovery_timeout_ms = INITIAL_DISCOVERY_MS;
    cfg.command_timeout_ms = CH32_MPU6050_GATEWAY_TIMEOUT_MS;
    if (ch32_i2c_multi_init(&cfg) != 0) {
        LOGE("CAN core init failed");
        return;
    }

    discover_missing_module(INITIAL_DISCOVERY_MS);
    while (s_imu == NULL) {
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        discover_missing_module(REDISCOVERY_TIMEOUT_MS);
        retry_delay_ms = retry_delay_ms < REDISCOVERY_FAST_MAX_MS
                             ? retry_delay_ms * 2U
                             : REDISCOVERY_MISSING_MS;
    }

    start_tick = xTaskGetTickCount();
    last_wake = start_tick;
    while ((TickType_t)(xTaskGetTickCount() - start_tick) <
           pdMS_TO_TICKS(EXAMPLE_DURATION_MS)) {
        ch32_mpu6050_orientation_t orientation;
        int result = ch32_mpu6050_read_orientation(s_imu, &orientation);

        if (result != 0) {
            LOGE("module run failed token=0x%04X node=%u err=%d",
                 s_active_node->token, s_active_node->node_id, result);
            if (++consecutive_failures >= 3U) {
                (void)ch32_mpu6050_deinit(s_imu);
                s_imu = NULL;
                s_active_node = NULL;
                retry_delay_ms = REDISCOVERY_FAST_FIRST_MS;
                while (s_imu == NULL) {
                    vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
                    discover_missing_module(REDISCOVERY_TIMEOUT_MS);
                    retry_delay_ms = retry_delay_ms < REDISCOVERY_FAST_MAX_MS
                                         ? retry_delay_ms * 2U
                                         : REDISCOVERY_MISSING_MS;
                }
                consecutive_failures = 0U;
                last_wake = xTaskGetTickCount();
                continue;
            }
        } else {
            consecutive_failures = 0U;
            if (++calculations % PRINT_DIVIDER == 0U) {
                LOGI("roll=%.2fdeg pitch=%.2fdeg yaw=%.2fdeg valid=%u accel=%u",
                     orientation.roll_deg, orientation.pitch_deg,
                     orientation.yaw_deg, orientation.valid ? 1U : 0U,
                     orientation.accel_correction_used ? 1U : 0U);
                ++prints;
            }
        }
        if ((TickType_t)(xTaskGetTickCount() - last_wake) >=
            pdMS_TO_TICKS(CALCULATION_MS)) {
            last_wake = xTaskGetTickCount();
        } else {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CALCULATION_MS));
        }
    }

    if (ch32_mpu6050_deinit(s_imu) != 0) {
        LOGE("deinit failed token=0x%04X", s_active_node->token);
    }
    LOGI("test complete, duration_ms=%u printed=%u",
         EXAMPLE_DURATION_MS, (unsigned)prints);
}
