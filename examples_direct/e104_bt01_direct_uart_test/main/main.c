#include <stdint.h>

#include "e104_bt01_direct_uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AT_RESPONSE_CAPACITY       64U
#define AT_TRANSACTION_TIMEOUT_MS  500U
#define UART_RX_TIMEOUT_MS         100U
#define UART_TX_TIMEOUT_MS         100U
#define TEST_FRAME_PERIOD_MS       2000U

static const char *TAG = "e104_bt01_test";

static esp_err_t run_at_command(const char *command)
{
    char response[AT_RESPONSE_CAPACITY] = {0};
    e104_bt01_response_t result = {0};

    esp_err_t err = e104_bt01_direct_uart_at_transaction(command,
                                                         response,
                                                         sizeof(response),
                                                         &result,
                                                         AT_TRANSACTION_TIMEOUT_MS);
    if (err != ESP_OK) {
        if (result.type == E104_BT01_RESPONSE_MODULE_ERROR) {
            ESP_LOGE(TAG, "%s failed: module error %d", command, result.module_error);
        } else {
            ESP_LOGE(TAG, "%s failed: %s", command, esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGI(TAG, "%s response (%u bytes): %s",
             command,
             (unsigned)result.length,
             response);
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "CN1: 1=3.3V, 2=GPIO17 TX, 3=GPIO16 RX, 4=GND");
    ESP_LOGI(TAG, "AT test requires the module awake, unconnected, and at 19200 8N1");

    e104_bt01_direct_uart_config_t config = {0};
    e104_bt01_direct_uart_get_default_config(&config);

    esp_err_t err = e104_bt01_direct_uart_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "driver init failed: %s", esp_err_to_name(err));
        return;
    }

    err = run_at_command("AT");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AT probe stopped; check power, TX/RX, switch state, and baud rate");
        return;
    }

    err = run_at_command("AT+BAUD?");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "baud query failed; transparent test not started");
        return;
    }

    ESP_LOGI(TAG, "AT probe passed; connect E104-BT01 and use FFF1 notify / FFF2 write");

    static const uint8_t test_frame[] = "ESP32-BLE-UART-TEST";
    _Static_assert(sizeof(test_frame) - 1U <= E104_BT01_BLE_PAYLOAD_LENGTH,
                   "test frame exceeds the E104-BT01 BLE payload length");
    uint8_t rx_buffer[64] = {0};
    TickType_t last_tx_tick = xTaskGetTickCount();

    while (true) {
        int received = e104_bt01_direct_uart_read(rx_buffer,
                                                  sizeof(rx_buffer),
                                                  UART_RX_TIMEOUT_MS);
        if (received < 0) {
            ESP_LOGE(TAG, "UART read failed");
            break;
        }
        if (received > 0) {
            ESP_LOGI(TAG, "received %d bytes from FFF2", received);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, rx_buffer, received, ESP_LOG_INFO);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_tx_tick) >= pdMS_TO_TICKS(TEST_FRAME_PERIOD_MS)) {
            err = e104_bt01_direct_uart_write(test_frame,
                                               sizeof(test_frame) - 1U,
                                               UART_TX_TIMEOUT_MS);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "UART write failed: %s", esp_err_to_name(err));
                break;
            }
            ESP_LOGI(TAG, "sent %u-byte test frame to FFF1",
                     (unsigned)(sizeof(test_frame) - 1U));
            last_tx_tick = now;
        }
    }
}
