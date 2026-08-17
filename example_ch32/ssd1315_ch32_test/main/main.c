#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ch32_i2c_multi_gateway_final.h"
#include "ch32_ssd1315_gateway.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_NODES 6U
#define DISCOVERY_START_MS 1000U
#define DISCOVERY_RETRY_MS 1000U
#define REDISCOVERY_MISSING_MS 10000U
#define REDISCOVERY_FAST_FIRST_MS 250U
#define REDISCOVERY_FAST_MAX_MS 2000U
#define EXAMPLE_DURATION_MS 60000U

static ch32_i2c_multi_node_t s_nodes[MAX_NODES];
static size_t s_node_count;
static ch32_ssd1315_handle_t s_display;

static bool try_bind_first_display(void)
{
    for (size_t i = 0U; i < s_node_count; ++i) {
        ch32_i2c_multi_node_t *node = &s_nodes[i];
        const ch32_ssd1315_cfg_t cfg = {
            .i2c_addr = CH32_SSD1315_I2C_ADDR,
            .clk_speed_hz = CH32_SSD1315_I2C_FREQ_HZ,
            .ch32_node = node,
        };
        int result;

        if (!node->ready ||
            node->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C) {
            continue;
        }
        result = ch32_ssd1315_init(&s_display, &cfg);
        if (result == 0) {
            result = ch32_ssd1315_draw_text(
                s_display, 0U, 0U, "SSD1315 OK");
        }
        if (result == 0) {
            result = ch32_ssd1315_refresh(s_display);
        }
        if (result == 0) {
            printf("[INF][SSD1315_EXAMPLE] display ready token=0x%04X node=%u\n",
                   node->token, node->node_id);
            return true;
        }
        if (s_display != NULL) {
            (void)ch32_ssd1315_deinit(s_display);
            s_display = NULL;
        }
        printf("[WRN][SSD1315_EXAMPLE] candidate rejected token=0x%04X node=%u err=%d\n",
               node->token, node->node_id, result);
    }
    return false;
}

static void discover_missing_display(uint32_t timeout_ms)
{
    ch32_i2c_multi_result_t result = ch32_i2c_multi_discover_incremental(
        s_nodes, MAX_NODES, &s_node_count, timeout_ms);

    printf("[INF][SSD1315_EXAMPLE] discovery result=%s candidates=%u\n",
           ch32_i2c_multi_result_text(result), (unsigned)s_node_count);
    if (s_display == NULL && !try_bind_first_display()) {
        printf("[WRN][SSD1315_EXAMPLE] SSD1315 not found; retry later\n");
    }
}

void app_main(void)
{
    ch32_i2c_multi_config_t cfg;
    uint32_t retry_delay_ms = REDISCOVERY_FAST_FIRST_MS;

    ch32_i2c_multi_default_config(&cfg);
    cfg.discovery_timeout_ms = DISCOVERY_START_MS;
    cfg.command_timeout_ms = CH32_SSD1315_GATEWAY_TIMEOUT_MS;
    if (ch32_i2c_multi_init(&cfg) != 0) {
        printf("[ERR][SSD1315_EXAMPLE] shared CAN gateway init failed\n");
        return;
    }

    discover_missing_display(DISCOVERY_START_MS);
    while (s_display == NULL) {
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        discover_missing_display(DISCOVERY_RETRY_MS);
        retry_delay_ms = retry_delay_ms < REDISCOVERY_FAST_MAX_MS
                             ? retry_delay_ms * 2U
                             : REDISCOVERY_MISSING_MS;
    }

    printf("[INF][SSD1315_EXAMPLE] display ready; observing for %u ms\n",
           EXAMPLE_DURATION_MS);
    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_DURATION_MS));
    if (ch32_ssd1315_deinit(s_display) != 0) {
        printf("[ERR][SSD1315_EXAMPLE] deinit failed\n");
    }
    s_display = NULL;
    printf("[INF][SSD1315_EXAMPLE] example complete\n");
}
