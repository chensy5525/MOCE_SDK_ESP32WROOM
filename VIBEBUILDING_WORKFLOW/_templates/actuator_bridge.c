/**
 * @file    <abbr>_bridge.c
 * @brief   <Module Name> CH32 bridge actuator skeleton
 *
 * Generate this package only when authoritative CH32 firmware and a matching
 * ESP32 gateway protocol layer implement every required physical operation.
 * Do not invent can_send_gpio_write(), can_send_pwm_duty() or similar APIs.
 */

#include "<abbr>.h"

#include <stdbool.h>
#include <stdint.h>

/* Include exactly the verified protocol header for the actual downstream
 * interface.  cfg/ctx holds that layer's stable-node pointer, never a copied
 * runtime node_id.  The discovery layer owns F0/F1/F2 and updates node_id in
 * place.  The actuator driver owns safe-state and module behavior only. */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    /* Validate stable node and downstream identity, drive all outputs to the
     * documented safe state, then publish the initialized handle. */
    return ERR_NOT_SUPPORTED;
}

int <abbr>_deinit(<abbr>_handle_t handle)
{
    /* Restore safe state before releasing only this module instance. */
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_write(<abbr>_handle_t handle, uint8_t channel,
                 <value_type> value)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}
