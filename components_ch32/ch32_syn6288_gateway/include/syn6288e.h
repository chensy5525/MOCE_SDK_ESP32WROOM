#ifndef SYN6288E_H
#define SYN6288E_H

#include "ch32_uart_dynamic_gateway_final.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "module_errors.h"

#define SYN6288E_UART_BAUD_RATE      9600U
#define SYN6288E_MAX_FRAME_LENGTH    206U
#define SYN6288E_MAX_TEXT_LENGTH     200U

typedef struct {
    ch32_uart_dynamic_node_t *ch32_node;
} syn6288e_config_t;

typedef struct {
    ch32_uart_dynamic_node_t *ch32_node;
    bool initialized;
} syn6288e_t;

int syn6288e_init(syn6288e_t *device, const syn6288e_config_t *config);
int syn6288e_deinit(syn6288e_t *device);
/* Success means CH32 confirmed delivery of the complete UART frame.  It does
 * not imply SYN6288E acceptance or playback completion. */
int syn6288e_send_raw(syn6288e_t *device, const uint8_t *frame,
                      size_t length);
int syn6288e_speak_gbk(syn6288e_t *device, const uint8_t *text,
                       size_t text_length);

#endif
