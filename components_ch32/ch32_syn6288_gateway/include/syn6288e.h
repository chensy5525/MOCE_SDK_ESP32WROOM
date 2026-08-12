#ifndef SYN6288E_H
#define SYN6288E_H

#include "ch32_uart_dynamic_gateway_final.h"

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

#define SYN6288E_UART_BAUD_RATE      9600U
#define SYN6288E_MAX_FRAME_LENGTH    4096U

typedef enum {
    SYN6288E_STATE_NOT_INIT = 0,
    SYN6288E_STATE_READY,
    SYN6288E_STATE_FAULT,
} syn6288e_state_t;

typedef struct {
    ch32_uart_dynamic_node_t *ch32_node;
} syn6288e_config_t;

typedef struct {
    ch32_uart_dynamic_node_t *ch32_node;
    syn6288e_state_t state;
    bool initialized;
} syn6288e_t;

int syn6288e_init(syn6288e_t *device, const syn6288e_config_t *config);
int syn6288e_deinit(syn6288e_t *device);
int syn6288e_send_raw(syn6288e_t *device, const uint8_t *frame,
                      size_t length);
int syn6288e_speak_danger(syn6288e_t *device);
int syn6288e_get_state(const syn6288e_t *device, syn6288e_state_t *state);

#endif
