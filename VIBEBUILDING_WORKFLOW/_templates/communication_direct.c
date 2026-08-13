/**
 * @file    <abbr>_direct.c
 * @brief   <Module Name> direct communication-driver skeleton
 */

#include "<abbr>.h"
#include "board.h"
#include "bsp_uart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Read bsp_uart.h before implementation. UART0 is reserved for logs; use the
 * board-defined business UART. Default to a fixed instance/ring-buffer pool. */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    return ERR_NOT_SUPPORTED;
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
