/**
 * @file    <abbr>.h
 * @brief   <Module Name> public actuator API skeleton
 */

#ifndef <FILENAME_UPPER>_H__
#define <FILENAME_UPPER>_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Do not include driver/i2c.h, driver/uart.h or driver/gpio.h here.  Direct
 * implementations use the current BSP headers; bridge packages use only an
 * already implemented and verified gateway protocol layer. */

#define <ABBR>_SAFE_STATE             <val>
#define <ABBR>_ACTION_DELAY_MS        <val>U
#define <ABBR>_MAX_RETRIES            <val>U

typedef struct {
    uint8_t num_channels;
    uint8_t safe_state;
    uint32_t timeout_ms;
    /* Add mode-specific direct configuration or the correct stable-node
     * pointer in the independently generated package. */
} <abbr>_cfg_t;

typedef struct <abbr>_ctx *<abbr>_handle_t;

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg);
int <abbr>_deinit(<abbr>_handle_t handle);
int <abbr>_write(<abbr>_handle_t handle, uint8_t channel,
                 <value_type> value);
int <abbr>_read(<abbr>_handle_t handle, uint8_t channel,
                <status_type> *status);
int <abbr>_reset(<abbr>_handle_t handle);

#define ERR_<ABBR>_INVALID_CHANNEL    -10
#define ERR_<ABBR>_ACTION_FAILED      -11

#endif /* <FILENAME_UPPER>_H__ */
