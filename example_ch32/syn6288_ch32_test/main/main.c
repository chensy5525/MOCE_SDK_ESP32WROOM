#include "ch32_i2c_multi_gateway_final.h"
#include "ch32_uart_dynamic_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "syn6288e.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SYN6288E_EXAMPLE_MAX_NODES          6U
#define SYN6288E_EXAMPLE_DISCOVERY_MS       3000U
#define SYN6288E_EXAMPLE_REDISCOVERY_MS     3000U
#define SYN6288E_EXAMPLE_BROADCAST_MS       1000U
#define SYN6288E_EXAMPLE_INVENTORY_MS       5000U

static ch32_uart_dynamic_node_t s_nodes[SYN6288E_EXAMPLE_MAX_NODES];
static size_t s_node_count;
static syn6288e_t s_speech;
static ch32_uart_dynamic_node_t *s_active_node;

static ch32_uart_dynamic_node_t *syn6288e_find_ready_node(void)
{
    size_t index;
    for (index = 0U; index < s_node_count; ++index) {
        if (s_nodes[index].ready) {
            return &s_nodes[index];
        }
    }
    return NULL;
}

static bool syn6288e_discover_and_bind(void)
{
    ch32_uart_dynamic_result_t discovery_result;
    syn6288e_config_t config;
    ch32_uart_dynamic_node_t *node;
    int result;

    discovery_result = ch32_uart_dynamic_discover_incremental(
        s_nodes, SYN6288E_EXAMPLE_MAX_NODES, &s_node_count);
    printf("[INF][SYN6288E_EXAMPLE] discovery result=%s count=%u\n",
           ch32_uart_dynamic_result_text(discovery_result),
           (unsigned)s_node_count);
    node = syn6288e_find_ready_node();
    if (node == NULL) {
        printf("[WRN][SYN6288E_EXAMPLE] no ready CH32 UART gateway\n");
        return false;
    }
    if (s_active_node == node && s_speech.initialized) {
        return true;
    }
    if (s_speech.initialized) {
        (void)syn6288e_deinit(&s_speech);
    }
    config.ch32_node = node;
    result = syn6288e_init(&s_speech, &config);
    if (result != 0) {
        printf("[ERR][SYN6288E_EXAMPLE] init failed token=0x%04X node=%u err=%d\n",
               node->token, node->node_id, result);
        return false;
    }
    s_active_node = node;
    return true;
}

static void syn6288e_inventory_task(void *argument)
{
    (void)argument;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SYN6288E_EXAMPLE_INVENTORY_MS));
        ch32_uart_dynamic_result_t result =
            ch32_uart_dynamic_discover_incremental(
                s_nodes, SYN6288E_EXAMPLE_MAX_NODES, &s_node_count);
        printf("[INF][SYN6288E_EXAMPLE] inventory result=%s count=%u\n",
               ch32_uart_dynamic_result_text(result),
               (unsigned)s_node_count);
    }
}

void app_main(void)
{
    ch32_i2c_multi_config_t can_config;
    ch32_uart_dynamic_config_t uart_config;
    TickType_t last_wake_time;

    ch32_i2c_multi_default_config(&can_config);
    if (ch32_i2c_multi_init(&can_config) != 0) {
        printf("[ERR][SYN6288E_EXAMPLE] shared CAN gateway init failed\n");
        return;
    }
    ch32_uart_dynamic_default_config(&uart_config);
    uart_config.discovery_window_ms = SYN6288E_EXAMPLE_DISCOVERY_MS;
    if (ch32_uart_dynamic_init(&uart_config) != 0) {
        printf("[ERR][SYN6288E_EXAMPLE] UART gateway layer init failed\n");
        return;
    }

    while (!syn6288e_discover_and_bind()) {
        vTaskDelay(pdMS_TO_TICKS(SYN6288E_EXAMPLE_REDISCOVERY_MS));
    }
    if (xTaskCreate(syn6288e_inventory_task, "syn_inventory", 4096U,
                    NULL, 4U, NULL) != pdPASS) {
        printf("[ERR][SYN6288E_EXAMPLE] inventory task create failed\n");
        return;
    }

    last_wake_time = xTaskGetTickCount();
    while (true) {
        int result = syn6288e_speak_danger(&s_speech);
        if (result != 0) {
            printf("[WRN][SYN6288E_EXAMPLE] speak danger failed err=%d\n",
                   result);
        }
        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(SYN6288E_EXAMPLE_BROADCAST_MS));
    }
}
