#include "syn6288_direct.h"

#include <stdint.h>

#include "bsp_uart.h"

esp_err_t syn6288_direct_init(void)
{
    bsp_uart_config_t config = {0};
    bsp_uart_get_default_config(&config);
    config.tx_gpio = 17;
    config.rx_gpio = 16;
    config.baud_rate = 9600;
    return bsp_uart_init(&config);
}

esp_err_t syn6288_direct_speak_test(void)
{
    static const uint8_t frame[] = {
        0xFD, 0x00, 0x07, 0x01, 0x01,
        0xB2, 0xE2, 0xCA, 0xD4, 0xB4,
    };
    return bsp_uart_write(frame, sizeof(frame), 100U);
}
