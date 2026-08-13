/**
 * @file    <abbr>_bridge.c
 * @brief   <Module Name> CH32 bridge driver skeleton
 *
 * Generate this as an independent package.  Select exactly one verified
 * protocol layer from the real repository:
 *   I2C  -> ch32_i2c_multi_gateway_final.h
 *   UART -> ch32_uart_dynamic_gateway_final.h
 * Never include ch32_can_gateway_core.h or construct CAN frames here.
 */

#include "<abbr>.h"

/* I2C example only; use the UART stable-node/API types for a UART sensor. */
#include "ch32_i2c_multi_gateway_final.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct <abbr>_ctx {
    ch32_i2c_multi_node_t *ch32_node;
    uint8_t device_addr;
    uint32_t bridge_timeout_ms;
    bool initialized;
} <abbr>_ctx_t;

static int <abbr>_validate_node(const ch32_i2c_multi_node_t *node)
{
    if (node == NULL || !node->ready || node->token == 0U ||
        node->node_id < 0x01U || node->node_id > 0x20U) {
        return ERR_INVALID_PARAM;
    }
    return 0;
}

/* Use real gateway operations such as:
 * ch32_i2c_multi_probe()
 * ch32_i2c_multi_read_regs_from()
 * ch32_i2c_multi_write_reg_to()
 * ch32_i2c_multi_write_multi_to()
 *
 * The driver never sends F0/F1/F2.  Discovery owns the stable record and
 * updates node_id in place; every operation follows ctx->ch32_node.
 */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    /* Validate handle/cfg, validate the stable node, claim a static instance,
     * probe/identify the downstream device, configure it, then publish handle.
     * Map gateway errors according to error-codes.md and log the exact stage. */
    return ERR_NOT_SUPPORTED;
}

int <abbr>_deinit(<abbr>_handle_t handle)
{
    /* Release only this module instance.  Never stop TWAI or release the CH32
     * node because the gateway may be shared by other downstream modules. */
    return handle == NULL ? ERR_INVALID_PARAM : 0;
}

int <abbr>_read(<abbr>_handle_t handle, <data_type> *data)
{
    return handle == NULL || data == NULL ? ERR_INVALID_PARAM
                                          : ERR_NOT_SUPPORTED;
}
