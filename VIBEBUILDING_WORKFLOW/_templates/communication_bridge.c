/**
 * @file    <abbr>_bridge.c
 * @brief   <Module Name> CH32-UART bridge driver skeleton
 *
 * This module driver depends on ch32_uart_dynamic_gateway_final directly.
 * It must never depend on ch32_i2c_multi_gateway_final or the CAN core.
 */

#include "<abbr>.h"
#include "ch32_uart_dynamic_gateway_final.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct <abbr>_ctx {
    ch32_uart_dynamic_node_t *ch32_node;
    uint32_t bridge_timeout_ms;
    bool initialized;
} <abbr>_ctx_t;

static int <abbr>_validate_node(const ch32_uart_dynamic_node_t *node)
{
    if (node == NULL || !node->ready || node->token == 0U ||
        node->node_id < 0x31U || node->node_id > 0x4FU) {
        return ERR_INVALID_PARAM;
    }
    return 0;
}

/* Use only current public APIs read from the target repository, including:
 * ch32_uart_dynamic_send(), ch32_uart_dynamic_set_baud() and
 * ch32_uart_dynamic_poll_rx().  The gateway layer owns START/DATA/ACK, CRC,
 * receive routing and F0/F1/F2.  The module driver owns only its downstream
 * device protocol, identification, state and error mapping.
 */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    return ERR_NOT_SUPPORTED;
}

int <abbr>_deinit(<abbr>_handle_t handle)
{
    return handle == NULL ? ERR_INVALID_PARAM : 0;
}

int <abbr>_write(<abbr>_handle_t handle, const uint8_t *data, size_t len)
{
    return handle == NULL || data == NULL || len == 0U
               ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_read(<abbr>_handle_t handle, uint8_t *buf, size_t len,
                uint32_t timeout_ms)
{
    return handle == NULL || buf == NULL || len == 0U || timeout_ms == 0U
               ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}
