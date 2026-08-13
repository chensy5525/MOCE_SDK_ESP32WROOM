/**
 * @file    <abbr>.h
 * @brief   <Module Name> public driver API skeleton
 * @note    Generate an independent header for each direct/bridge package.
 */

#ifndef <FILENAME_UPPER>_H__
#define <FILENAME_UPPER>_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Keep ESP-IDF driver types out of the public API.  The direct implementation
 * reads board.h and the real bsp_<transport>.h.  A bridge header may include
 * ch32_i2c_multi_gateway_final.h or ch32_uart_dynamic_gateway_final.h only when
 * its cfg publicly contains the corresponding stable-node pointer type. */

#define <ABBR>_I2C_ADDR_DEFAULT       0x<val>U
#define <ABBR>_BUS_SPEED_HZ           <val>U
#define <ABBR>_STARTUP_MS             <val>U
#define <ABBR>_MAX_RETRIES            <val>U
#define <ABBR>_RANGE_MIN              <val>
#define <ABBR>_RANGE_MAX              <val>

typedef struct {
    uint8_t i2c_addr;
    uint32_t bus_speed_hz;
    uint32_t timeout_ms;
    /* Direct-only pins or bridge-only stable-node pointer are added to the
     * independently generated package, based on card.md and real APIs. */
} <abbr>_cfg_t;

typedef struct <abbr>_ctx *<abbr>_handle_t;

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg);
int <abbr>_deinit(<abbr>_handle_t handle);
int <abbr>_read(<abbr>_handle_t handle, <data_type> *data);
int <abbr>_reset(<abbr>_handle_t handle);

#define ERR_<ABBR>_OUT_OF_RANGE       -10

#endif /* <FILENAME_UPPER>_H__ */
