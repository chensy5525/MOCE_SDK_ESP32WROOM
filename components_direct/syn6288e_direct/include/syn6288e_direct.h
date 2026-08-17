#ifndef SYN6288E_DIRECT_H
#define SYN6288E_DIRECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "module_errors.h"

#define SYN6288E_DIRECT_BAUD_RATE       9600U
#define SYN6288E_DIRECT_TX_TIMEOUT_MS   1000U
#define SYN6288E_DIRECT_MAX_FRAME_LEN   206U
#define SYN6288E_DIRECT_MAX_TEXT_LEN    200U

typedef struct {
    uint32_t tx_timeout_ms;
} syn6288e_direct_config_t;

typedef struct {
    uint32_t tx_timeout_ms;
    bool initialized;
} syn6288e_direct_t;

#define SYN6288E_DIRECT_CONFIG_DEFAULT() \
    { .tx_timeout_ms = SYN6288E_DIRECT_TX_TIMEOUT_MS }

int syn6288e_direct_init(syn6288e_direct_t *device,
                         const syn6288e_direct_config_t *config);
int syn6288e_direct_deinit(syn6288e_direct_t *device);
/* Success means the complete frame was delivered to the UART.  It does not
 * imply SYN6288E acceptance or playback completion. */
int syn6288e_direct_send_raw(syn6288e_direct_t *device,
                             const uint8_t *frame, size_t length);
int syn6288e_direct_speak_gbk(syn6288e_direct_t *device,
                              const uint8_t *text, size_t text_length);

#endif
