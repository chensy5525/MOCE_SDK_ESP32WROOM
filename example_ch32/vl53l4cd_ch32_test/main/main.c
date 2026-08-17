#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ch32_i2c_multi_gateway_final.h"
#include "ch32_vl53l4cd_gateway.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "VL53L4CD_EXAMPLE"
#define MAX_NODES 6U
#define DISCOVERY_START_MS 1000U
#define DISCOVERY_RETRY_MS 1000U
#define REDISCOVERY_MISSING_MS 10000U
#define REDISCOVERY_FAST_FIRST_MS 250U
#define REDISCOVERY_FAST_MAX_MS 2000U
#define SAMPLE_PERIOD_MS 200U
#define EXAMPLE_DURATION_MS 60000U

static ch32_i2c_multi_node_t s_nodes[MAX_NODES];
static size_t s_node_count;
static ch32_i2c_multi_node_t *s_active_node;
static ch32_vl53l4cd_handle_t s_sensor;

static bool is_recovery_error(int result)
{
    return result == ERR_TIMEOUT || result == ERR_NO_DEVICE ||
           result == ERR_CH32_VL53L4CD_COMM ||
           result == ERR_CH32_VL53L4CD_DATA_NOT_READY;
}

static bool try_bind_first_sensor(void)
{
    for (size_t i = 0U; i < s_node_count; ++i) {
        ch32_i2c_multi_node_t *node = &s_nodes[i];
        const ch32_vl53l4cd_cfg_t cfg = {
            .i2c_addr = CH32_VL53L4CD_I2C_ADDR_DEFAULT,
            .ch32_node = node,
            .data_ready_timeout_ms = CH32_VL53L4CD_DATA_READY_TIMEOUT_MS,
        };
        int result;

        if (!node->ready ||
            node->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C) {
            continue;
        }
        result = ch32_vl53l4cd_init(&s_sensor, &cfg);
        if (result == 0) {
            s_active_node = node;
            printf("[INF][" TAG "] sensor ready token=0x%04X node=%u\n",
                   node->token, node->node_id);
            return true;
        }
        printf("[WRN][" TAG "] candidate rejected token=0x%04X node=%u err=%d\n",
               node->token, node->node_id, result);
    }
    return false;
}

static void discover_missing_sensor(uint32_t timeout_ms)
{
    ch32_i2c_multi_result_t result = ch32_i2c_multi_discover_incremental(
        s_nodes, MAX_NODES, &s_node_count, timeout_ms);

    printf("[INF][" TAG "] discovery result=%s candidates=%u\n",
           ch32_i2c_multi_result_text(result), (unsigned)s_node_count);
    if (s_sensor == NULL && !try_bind_first_sensor()) {
        printf("[WRN][" TAG "] VL53L4CD not found; retry later\n");
    }
}

void app_main(void)
{
    ch32_i2c_multi_config_t cfg;
    uint32_t retry_delay_ms = REDISCOVERY_FAST_FIRST_MS;
    uint32_t consecutive_failures = 0U;
    uint32_t samples = 0U;
    TickType_t start_tick;
    TickType_t last_wake;

    ch32_i2c_multi_default_config(&cfg);
    cfg.discovery_timeout_ms = DISCOVERY_START_MS;
    cfg.command_timeout_ms = CH32_VL53L4CD_GATEWAY_TIMEOUT_MS;
    if (ch32_i2c_multi_init(&cfg) != 0) {
        printf("[ERR][CH32_CAN_CORE] init FAIL\n");
        return;
    }

    discover_missing_sensor(DISCOVERY_START_MS);
    while (s_sensor == NULL) {
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        discover_missing_sensor(DISCOVERY_RETRY_MS);
        retry_delay_ms = retry_delay_ms < REDISCOVERY_FAST_MAX_MS
                             ? retry_delay_ms * 2U
                             : REDISCOVERY_MISSING_MS;
    }

    start_tick = xTaskGetTickCount();
    last_wake = start_tick;
    while ((TickType_t)(xTaskGetTickCount() - start_tick) <
           pdMS_TO_TICKS(EXAMPLE_DURATION_MS)) {
        ch32_vl53l4cd_result_t data;
        int result = ch32_vl53l4cd_read(s_sensor, &data);

        if (result == 0) {
            consecutive_failures = 0U;
            ++samples;
            printf("[INF][VL53L4CD] data valid=1 mm=%u status=%u\n",
                   data.distance_mm, data.range_status);
        } else if (result == ERR_CH32_VL53L4CD_OUT_OF_RANGE) {
            consecutive_failures = 0U;
            ++samples;
            printf("[WRN][VL53L4CD] OUT RANGE mm=%u status=%u\n",
                   data.distance_mm, data.range_status);
        } else {
            printf("[ERR][VL53L4CD] runtime FAIL token=0x%04X node=%u err=%d\n",
                   s_active_node->token, s_active_node->node_id, result);
            if (is_recovery_error(result) &&
                ++consecutive_failures >= 3U) {
                (void)ch32_vl53l4cd_deinit(s_sensor);
                s_sensor = NULL;
                s_active_node = NULL;
                retry_delay_ms = REDISCOVERY_FAST_FIRST_MS;
                while (s_sensor == NULL) {
                    vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
                    discover_missing_sensor(DISCOVERY_RETRY_MS);
                    retry_delay_ms = retry_delay_ms < REDISCOVERY_FAST_MAX_MS
                                         ? retry_delay_ms * 2U
                                         : REDISCOVERY_MISSING_MS;
                }
                consecutive_failures = 0U;
                last_wake = xTaskGetTickCount();
                continue;
            }
        }
        if ((TickType_t)(xTaskGetTickCount() - last_wake) >=
            pdMS_TO_TICKS(SAMPLE_PERIOD_MS)) {
            last_wake = xTaskGetTickCount();
        } else {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        }
    }

    (void)ch32_vl53l4cd_deinit(s_sensor);
    printf("[INF][" TAG "] example complete duration_ms=%u samples=%u\n",
           EXAMPLE_DURATION_MS, (unsigned)samples);
}
