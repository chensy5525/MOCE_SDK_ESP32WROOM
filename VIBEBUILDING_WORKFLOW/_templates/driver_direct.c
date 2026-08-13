/**
 * @file    <abbr>_direct.c
 * @brief   <Module Name> direct driver-board skeleton
 */

#include "<abbr>.h"
#include "board.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"

#include <stdbool.h>
#include <stdint.h>

/* Verify actual BSP signatures and board resource allocation before writing
 * this driver. Default to a static instance pool and establish a safe state
 * before enabling power/PWM. */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    return ERR_NOT_SUPPORTED;
}

int <abbr>_write(<abbr>_handle_t handle, int32_t speed, bool direction)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_estop(<abbr>_handle_t handle)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}
