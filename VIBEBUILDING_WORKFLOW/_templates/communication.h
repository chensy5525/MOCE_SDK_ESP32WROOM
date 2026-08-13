/**
 * @file    <abbr>.h
 * @brief   <Module Name> public communication-module API skeleton
 */

#ifndef <FILENAME_UPPER>_H__
#define <FILENAME_UPPER>_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The direct implementation uses board.h + bsp_uart.h.  The CH32-UART bridge
 * package uses ch32_uart_dynamic_gateway_final and a stable UART node pointer.
 * Do not expose driver/uart.h types in this public API. */

#define <ABBR>_UART_BAUD_DEFAULT      <val>U
#define <ABBR>_UART_BAUD_TARGET       <val>U
#define <ABBR>_RX_BUF_SIZE            <val>U
#define <ABBR>_RECONNECT_INTERVAL_MS  <val>U

typedef enum {
    <ABBR>_STATE_NOT_INIT,
    <ABBR>_STATE_READY,
    <ABBR>_STATE_CONNECTED,
    <ABBR>_STATE_FAULT,
} <abbr>_state_t;

typedef struct {
    uint32_t baud_default;
    uint32_t baud_target;
    uint16_t rx_buf_size;
    uint32_t timeout_ms;
    /* Add direct UART configuration or a stable UART-node pointer to the
     * independently generated package. */
} <abbr>_cfg_t;

typedef struct <abbr>_ctx *<abbr>_handle_t;

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg);
int <abbr>_deinit(<abbr>_handle_t handle);
int <abbr>_read(<abbr>_handle_t handle, uint8_t *buf, size_t len,
                uint32_t timeout_ms);
int <abbr>_write(<abbr>_handle_t handle, const uint8_t *data, size_t len);
int <abbr>_get_state(<abbr>_handle_t handle, <abbr>_state_t *state);
int <abbr>_reset(<abbr>_handle_t handle);

#define ERR_<ABBR>_NOT_CONNECTED      -10
#define ERR_<ABBR>_RX_BUF_OVERFLOW    -11

#endif /* <FILENAME_UPPER>_H__ */
