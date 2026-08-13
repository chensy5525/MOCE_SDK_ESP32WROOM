/**
 * @file    <abbr>_direct.c
 * @brief   <Module Name> direct actuator skeleton
 */

#include "<abbr>.h"
#include "board.h"
#include "bsp_<transport>.h"

#include <stdbool.h>
#include <stdint.h>

/* Read the current BSP header before implementation. Use a fixed-capacity
 * instance pool unless card.md explicitly permits init/deinit allocation. */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    /* Validate, claim a static instance and establish the documented safe
     * output state before publishing the handle. */
    return ERR_NOT_SUPPORTED;
}

int <abbr>_deinit(<abbr>_handle_t handle)
{
    /* Restore safe state before releasing the instance. */
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_write(<abbr>_handle_t handle, uint8_t channel,
                 <value_type> value)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}
