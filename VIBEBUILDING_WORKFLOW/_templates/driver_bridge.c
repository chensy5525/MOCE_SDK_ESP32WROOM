/**
 * @file    <abbr>_bridge.c
 * @brief   <Module Name> CH32 bridge driver-board skeleton
 *
 * A GPIO/PWM/SPI bridge is not assumed to exist.  Before generating code,
 * verify the CH32 firmware, ESP32 protocol component, command layouts,
 * feedback path, timeout behavior and emergency-stop semantics.
 */

#include "<abbr>.h"

#include <stdbool.h>
#include <stdint.h>

/* Include the real verified protocol header only.  Hold its stable-node
 * pointer instead of a copied node_id.  Never include ch32_can_gateway_core.h
 * and never create a second TWAI receiver in a module driver. */

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    /* Validate the stable node and feedback capability, establish the safe
     * state, and only then make the instance available to callers. */
    return ERR_NOT_SUPPORTED;
}

int <abbr>_write(<abbr>_handle_t handle, int32_t speed, bool direction)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_estop(<abbr>_handle_t handle)
{
    /* The actual remote cut-off command must be verified end-to-end. */
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}
