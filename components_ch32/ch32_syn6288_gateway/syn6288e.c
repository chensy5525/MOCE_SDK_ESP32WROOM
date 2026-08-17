#include "syn6288e.h"

#include <stdio.h>
#include <string.h>

#define SYN6288E_LOG_TAG "SYN6288E"

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
        return ERR_NO_DEVICE;
    }

    gateway_result = ch32_uart_dynamic_send(device->ch32_node, frame, length);
    result = syn6288e_map_gateway_result(gateway_result);
    if (result == 0) {
        printf("[INF][%s] transport=DELIVERED route=CH32_UART node=%u "
               "bytes=%u\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id,
               (unsigned)length);
    } else if (result == ERR_TIMEOUT) {
        printf("[WRN][%s] communication timeout node=%u result=%s\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id,
               ch32_uart_dynamic_result_text(gateway_result));
    } else {
        printf("[ERR][%s] transmit failed node=%u result=%s\n",
               SYN6288E_LOG_TAG, device->ch32_node->node_id,
               ch32_uart_dynamic_result_text(gateway_result));
    }
    return result;
}

int syn6288e_speak_gbk(syn6288e_t *device, const uint8_t *text,
                       size_t text_length)
{
    uint8_t frame[SYN6288E_MAX_FRAME_LENGTH];
    size_t frame_length;
    uint16_t data_length;
    uint8_t checksum = 0U;

    if (text == NULL || text_length == 0U ||
        text_length > SYN6288E_MAX_TEXT_LENGTH) {
        return ERR_INVALID_PARAM;
    }
    data_length = (uint16_t)text_length + 3U;
    frame_length = text_length + 6U;
    frame[0] = 0xFDU;
    frame[1] = (uint8_t)(data_length >> 8U);
    frame[2] = (uint8_t)data_length;
    frame[3] = 0x01U;
    frame[4] = 0x01U;
    memcpy(&frame[5], text, text_length);
    for (size_t index = 0U; index < frame_length - 1U; ++index) {
        checksum ^= frame[index];
    }
    frame[frame_length - 1U] = checksum;
    return syn6288e_send_raw(device, frame, frame_length);
}
