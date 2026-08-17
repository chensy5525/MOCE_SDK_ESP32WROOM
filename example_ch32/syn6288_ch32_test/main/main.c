#include "ch32_uart_dynamic_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "syn6288e.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SYN6288E_EXAMPLE_MAX_NODES          6U
#define SYN6288E_EXAMPLE_DISCOVERY_MS       1000U
#define SYN6288E_EXAMPLE_REDISCOVERY_MS     10000U
#define SYN6288E_EXAMPLE_FAST_FIRST_MS      250U
#define SYN6288E_EXAMPLE_FAST_MAX_MS        2000U
#define SYN6288E_EXAMPLE_DURATION_MS         60000U

static ch32_uart_dynamic_node_t s_nodes[SYN6288E_EXAMPLE_MAX_NODES];
static size_t s_node_count;
static syn6288e_t s_speech;
static ch32_uart_dynamic_node_t *s_active_node;
static const uint8_t s_danger_text_gbk[] = {0xCEU, 0xA3U, 0xCFU, 0xD5U};

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

void app_main(void)
{
    ch32_uart_dynamic_config_t uart_config;
    int result;
    uint32_t retry_delay_ms = SYN6288E_EXAMPLE_FAST_FIRST_MS;

    ch32_uart_dynamic_default_config(&uart_config);
    uart_config.discovery_window_ms = SYN6288E_EXAMPLE_DISCOVERY_MS;
    if (ch32_uart_dynamic_init(&uart_config) != 0) {
        printf("[ERR][SYN6288E_EXAMPLE] UART gateway layer init failed\n");
        return;
    }

    while (!syn6288e_discover_and_bind()) {
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        retry_delay_ms = retry_delay_ms < SYN6288E_EXAMPLE_FAST_MAX_MS
                             ? retry_delay_ms * 2U
                             : SYN6288E_EXAMPLE_REDISCOVERY_MS;
    }
    result = syn6288e_speak_gbk(&s_speech, s_danger_text_gbk,
                                sizeof(s_danger_text_gbk));
    if (result != 0) {
        printf("[WRN][SYN6288E_EXAMPLE] speak danger failed err=%d\n",
               result);
    } else {
        printf("[INF][SYN6288E_EXAMPLE] observing for %u ms\n",
               SYN6288E_EXAMPLE_DURATION_MS);
        vTaskDelay(pdMS_TO_TICKS(SYN6288E_EXAMPLE_DURATION_MS));
    }
    if (syn6288e_deinit(&s_speech) != 0) {
        printf("[ERR][SYN6288E_EXAMPLE] deinit failed\n");
    }
    printf("[INF][SYN6288E_EXAMPLE] test complete\n");
}
