#include "syn6288e_direct.h"

#include "board.h"
#include "bsp_uart.h"
#include "esp_err.h"

#include <stdio.h>
#include <string.h>

#define SYN6288E_DIRECT_LOG_TAG "SYN6288E"

int syn6288e_direct_init(syn6288e_direct_t *device,
                         const syn6288e_direct_config_t *config)
{
    bsp_uart_config_t uart_config;

    if (device == NULL || config == NULL || config->tx_timeout_ms == 0U) {
        return ERR_INVALID_PARAM;
    }
    memset(device, 0, sizeof(*device));
    bsp_uart_get_default_config(&uart_config);
    uart_config.port = BOARD_UART_PORT;
    uart_config.tx_gpio = BOARD_UART_TX_GPIO;
    uart_config.rx_gpio = BOARD_UART_RX_GPIO;
    uart_config.baud_rate = SYN6288E_DIRECT_BAUD_RATE;
    if (bsp_uart_init(&uart_config) != ESP_OK) {
        printf("[ERR][%s] init failed, route=DIRECT_UART\n",
               SYN6288E_DIRECT_LOG_TAG);
        return ERR_HW_FAULT;
    }

    device->tx_timeout_ms = config->tx_timeout_ms;
    device->initialized = true;
    printf("[INF][%s] init OK, route=DIRECT_UART port=%d tx=%d rx=%d baud=%u\n",
           SYN6288E_DIRECT_LOG_TAG, BOARD_UART_PORT, BOARD_UART_TX_GPIO,
           BOARD_UART_RX_GPIO, SYN6288E_DIRECT_BAUD_RATE);
    return 0;
}

int syn6288e_direct_deinit(syn6288e_direct_t *device)
{
    if (device == NULL || !device->initialized) {
        return ERR_NOT_INIT;
    }
    if (bsp_uart_deinit() != ESP_OK) {
        return ERR_HW_FAULT;
    }
    memset(device, 0, sizeof(*device));
    return 0;
}

int syn6288e_direct_send_raw(syn6288e_direct_t *device,
                             const uint8_t *frame, size_t length)
{
    esp_err_t result;

    if (device == NULL || !device->initialized) {
        return ERR_NOT_INIT;
    }
    if (frame == NULL || length == 0U ||
        length > SYN6288E_DIRECT_MAX_FRAME_LEN) {
        return ERR_INVALID_PARAM;
    }
    result = bsp_uart_write(frame, length, device->tx_timeout_ms);
    if (result == ESP_ERR_TIMEOUT) {
        printf("[WRN][%s] communication timeout route=DIRECT_UART\n",
               SYN6288E_DIRECT_LOG_TAG);
        return ERR_TIMEOUT;
    }
    if (result != ESP_OK) {
        printf("[ERR][%s] transmit failed route=DIRECT_UART err=%s\n",
               SYN6288E_DIRECT_LOG_TAG, esp_err_to_name(result));
        return ERR_HW_FAULT;
    }

    printf("[INF][%s] transport=DELIVERED route=DIRECT_UART bytes=%u\n",
           SYN6288E_DIRECT_LOG_TAG, (unsigned)length);
    return 0;
}

int syn6288e_direct_speak_gbk(syn6288e_direct_t *device,
                              const uint8_t *text, size_t text_length)
{
    uint8_t frame[SYN6288E_DIRECT_MAX_FRAME_LEN];
    size_t frame_length;
    uint16_t data_length;
    uint8_t checksum = 0U;

    if (text == NULL || text_length == 0U ||
        text_length > SYN6288E_DIRECT_MAX_TEXT_LEN) {
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
    return syn6288e_direct_send_raw(device, frame, frame_length);
}
