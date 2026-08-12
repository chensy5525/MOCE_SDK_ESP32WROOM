#ifndef SYN6288E_DIRECT_H
#define SYN6288E_DIRECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ERR_TIMEOUT                 (-1)
#define ERR_BUSY                    (-2)
#define ERR_NOT_INIT                (-3)
#define ERR_INVALID_PARAM           (-4)
#define ERR_NOT_SUPPORTED           (-5)
#define ERR_OVERFLOW                (-6)
#define ERR_NO_DEVICE               (-7)
#define ERR_HW_FAULT                (-8)

#define SYN6288E_DIRECT_BAUD_RATE       9600U
#define SYN6288E_DIRECT_TX_TIMEOUT_MS   1000U
#define SYN6288E_DIRECT_MAX_FRAME_LEN   4096U

typedef enum {
    SYN6288E_DIRECT_STATE_NOT_INIT = 0,
    SYN6288E_DIRECT_STATE_READY,
    SYN6288E_DIRECT_STATE_FAULT,
} syn6288e_direct_state_t;

typedef struct {
    uint32_t tx_timeout_ms;
} syn6288e_direct_config_t;

typedef struct {
    uint32_t tx_timeout_ms;
    syn6288e_direct_state_t state;
    bool initialized;
} syn6288e_direct_t;

#define SYN6288E_DIRECT_CONFIG_DEFAULT() \
    { .tx_timeout_ms = SYN6288E_DIRECT_TX_TIMEOUT_MS }

int syn6288e_direct_init(syn6288e_direct_t *device,
                         const syn6288e_direct_config_t *config);
int syn6288e_direct_deinit(syn6288e_direct_t *device);
int syn6288e_direct_send_raw(syn6288e_direct_t *device,
                             const uint8_t *frame, size_t length);
int syn6288e_direct_speak_danger(syn6288e_direct_t *device);
int syn6288e_direct_get_state(const syn6288e_direct_t *device,
                              syn6288e_direct_state_t *state);

#endif
