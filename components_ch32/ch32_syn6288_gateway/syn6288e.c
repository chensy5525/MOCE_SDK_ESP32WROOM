#include "syn6288e.h"

#include <stdio.h>
#include <string.h>

#define SYN6288E_LOG_TAG "SYN6288E"

/* SYN6288E GBK text frame for "危险". */
static const uint8_t SYN6288E_DANGER_FRAME[] = {
    0xFD, 0x00, 0x07, 0x01, 0x01,
    0xCE, 0xA3, 0xCF, 0xD5, 0x8D,
};

static int syn6288e_map_gateway_result(ch32_uart_dynamic_result_t result)
{
    switch (result) {
    case CH32_UART_DYNAMIC_OK:
        return 0;
    case CH32_UART_DYNAMIC_TIMEOUT:
        return ERR_TIMEOUT;
    case CH32_UART_DYNAMIC_BUSY:
        return ERR_BUSY;
    case CH32_UART_DYNAMIC_OVERFLOW:
        return ERR_OVERFLOW;
    case CH32_UART_DYNAMIC_NODE_NOT_FOUND:
    case CH32_UART_DYNAMIC_NODE_NOT_READY:
        return ERR_NO_DEVICE;
    case CH32_UART_DYNAMIC_LENGTH_ERROR:
        return ERR_INVALID_PARAM;
    case CH32_UART_DYNAMIC_PROTOCOL_ERROR:
        return ERR_NOT_SUPPORTED;
    case CH32_UART_DYNAMIC_COMM_FAIL:
    case CH32_UART_DYNAMIC_CRC_ERROR:
    default:
        return ERR_HW_FAULT;
    }
}

int syn6288e_init(syn6288e_t *device, const syn6288e_config_t *config)
{
    if (device == NULL || config == NULL || config->ch32_node == NULL) {
        return ERR_INVALID_PARAM;
    }
    if (!config->ch32_node->ready || config->ch32_node->token == 0U ||
        config->ch32_node->node_id < 0x31U ||
        config->ch32_node->node_id > 0x40U ||
        config->ch32_node->protocol_version !=
            CH32_UART_DYNAMIC_PROTOCOL_VERSION) {
        return ERR_NO_DEVICE;
    }

    memset(device, 0, sizeof(*device));
    device->ch32_node = config->ch32_node;
    device->state = SYN6288E_STATE_READY;
    device->initialized = true;
    printf("[INF][%s] init OK, token=0x%04X node=%u baud=%u\n",
           SYN6288E_LOG_TAG, device->ch32_node->token,
           device->ch32_node->node_id, SYN6288E_UART_BAUD_RATE);
    return 0;
}

int syn6288e_deinit(syn6288e_t *device)
{
    if (device == NULL || !device->initialized) {
        return ERR_NOT_INIT;
    }
    memset(device, 0, sizeof(*device));
    return 0;
}

int syn6288e_send_raw(syn6288e_t *device, const uint8_t *frame,
                      size_t length)
{
    ch32_uart_dynamic_result_t gateway_result;
    int result;

    if (device == NULL || !device->initialized) {
        return ERR_NOT_INIT;
    }
    if (frame == NULL || length == 0U ||
        length > SYN6288E_MAX_FRAME_LENGTH) {
        return ERR_INVALID_PARAM;
    }
    if (!device->ch32_node->ready) {
        device->state = SYN6288E_STATE_FAULT;
        return ERR_NO_DEVICE;
    }

    gateway_result = ch32_uart_dynamic_send(device->ch32_node, frame, length);
    result = syn6288e_map_gateway_result(gateway_result);
    if (result == 0) {
        device->state = SYN6288E_STATE_READY;
        printf("[INF][%s] SYN6288E_TX route=CH32_UART node=%u result=OK\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id);
    } else if (result == ERR_TIMEOUT) {
        printf("[WRN][%s] communication timeout node=%u result=%s\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id,
               ch32_uart_dynamic_result_text(gateway_result));
    } else {
        device->state = SYN6288E_STATE_FAULT;
        printf("[ERR][%s] transmit failed node=%u result=%s\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id,
               ch32_uart_dynamic_result_text(gateway_result));
    }
    return result;
}

int syn6288e_speak_danger(syn6288e_t *device)
{
    return syn6288e_send_raw(device, SYN6288E_DANGER_FRAME,
                             sizeof(SYN6288E_DANGER_FRAME));
}

int syn6288e_get_state(const syn6288e_t *device, syn6288e_state_t *state)
{
    if (device == NULL || state == NULL) {
        return ERR_INVALID_PARAM;
    }
    if (!device->initialized) {
        return ERR_NOT_INIT;
    }
    *state = device->state;
    return 0;
}
