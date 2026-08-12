#include "syn6288e_direct.h"

#include "board.h"
#include "bsp_uart.h"
#include "esp_err.h"

#include <stdio.h>
#include <string.h>

#define SYN6288E_DIRECT_LOG_TAG "SYN6288E"

/* SYN6288E frame containing the GBK text "危险" and XOR checksum 0x8D. */
static const uint8_t SYN6288E_DIRECT_DANGER_FRAME[] = {
    0xFD, 0x00, 0x07, 0x01, 0x01,
    0xCE, 0xA3, 0xCF, 0xD5, 0x8D,
};

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
        device->state = SYN6288E_DIRECT_STATE_FAULT;
        printf("[ERR][%s] init failed, route=DIRECT_UART\n",
               SYN6288E_DIRECT_LOG_TAG);
        return ERR_HW_FAULT;
    }

    device->tx_timeout_ms = config->tx_timeout_ms;
    device->state = SYN6288E_DIRECT_STATE_READY;
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
        device->state = SYN6288E_DIRECT_STATE_FAULT;
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
        device->state = SYN6288E_DIRECT_STATE_FAULT;
        printf("[ERR][%s] transmit failed route=DIRECT_UART err=%s\n",
               SYN6288E_DIRECT_LOG_TAG, esp_err_to_name(result));
        return ERR_HW_FAULT;
    }

    device->state = SYN6288E_DIRECT_STATE_READY;
    printf("[INF][%s] SYN6288E_TX route=DIRECT_UART result=OK\n",
           SYN6288E_DIRECT_LOG_TAG);
    return 0;
}

int syn6288e_direct_speak_danger(syn6288e_direct_t *device)
{
    return syn6288e_direct_send_raw(device, SYN6288E_DIRECT_DANGER_FRAME,
                                    sizeof(SYN6288E_DIRECT_DANGER_FRAME));
}

int syn6288e_direct_get_state(const syn6288e_direct_t *device,
                              syn6288e_direct_state_t *state)
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
