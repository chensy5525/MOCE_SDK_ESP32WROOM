/**
 * @file    <abbr>_direct.c
 * @brief   <Module Name> direct driver skeleton
 *
 * Before implementation, read board.h, the current bsp_<transport>.h and its
 * CMake component name. Do not guess BSP signatures or call ESP-IDF driver
 * APIs directly from the module driver.
 */

#include "<abbr>.h"
#include "board.h"
#include "bsp_<transport>.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define <ABBR>_MAX_INSTANCES  <val>U

typedef struct <abbr>_ctx {
    bool allocated;
    bool initialized;
    /* Store the real BSP device handle/config required by this module. */
} <abbr>_ctx_t;

static <abbr>_ctx_t s_instances[<ABBR>_MAX_INSTANCES];

static <abbr>_ctx_t *<abbr>_claim_instance(void)
{
    for (size_t index = 0U; index < <ABBR>_MAX_INSTANCES; ++index) {
        if (!s_instances[index].allocated) {
            memset(&s_instances[index], 0, sizeof(s_instances[index]));
            s_instances[index].allocated = true;
            return &s_instances[index];
        }
    }
    return NULL;
}

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg)
{
    /* Validate cfg, claim a static instance, initialize the real BSP device,
     * identify/configure the module, then publish handle. Release the instance
     * on every failure path. */
    return ERR_NOT_SUPPORTED;
}

int <abbr>_deinit(<abbr>_handle_t handle)
{
    return handle == NULL ? ERR_INVALID_PARAM : ERR_NOT_SUPPORTED;
}

int <abbr>_read(<abbr>_handle_t handle, <data_type> *data)
{
    return handle == NULL || data == NULL ? ERR_INVALID_PARAM
                                          : ERR_NOT_SUPPORTED;
}
