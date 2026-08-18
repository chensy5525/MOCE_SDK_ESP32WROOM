#include "e104_bt01_direct_uart.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_uart.h"
#include "esp_check.h"
#include "esp_timer.h"

static const char *TAG = "e104_bt01";

static bool s_initialized;
static e104_bt01_direct_uart_config_t s_active_config;

static uint32_t e104_bt01_remaining_ms(int64_t deadline_us)
{
    int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us <= 0) {
        return 0;
    }

    int64_t remaining_ms = (remaining_us + 999) / 1000;
    if (remaining_ms > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)remaining_ms;
}

static bool e104_bt01_has_crlf_suffix(const char *response, size_t length)
{
    return length >= 2U &&
           response[length - 2U] == '\r' &&
           response[length - 1U] == '\n';
}

static esp_err_t e104_bt01_parse_response(const char *response,
                                          size_t length,
                                          e104_bt01_response_t *result)
{
    if (!e104_bt01_has_crlf_suffix(response, length)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    result->length = length;
    result->module_error = 0;

    if (length == 5U && memcmp(response, "+OK\r\n", length) == 0) {
        result->type = E104_BT01_RESPONSE_OK;
        return ESP_OK;
    }

    if (length > 6U && memcmp(response, "+OK=", 4U) == 0) {
        result->type = E104_BT01_RESPONSE_OK_WITH_VALUE;
        return ESP_OK;
    }

    if (length > 7U && memcmp(response, "+ERR=", 5U) == 0) {
        char *number_end = NULL;
        errno = 0;
        long module_error = strtol(response + 5, &number_end, 10);
        if (errno != 0 || number_end != response + length - 2U ||
            module_error < INT_MIN || module_error > INT_MAX) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        result->type = E104_BT01_RESPONSE_MODULE_ERROR;
        result->module_error = (int)module_error;
        return ESP_FAIL;
    }

    return ESP_ERR_INVALID_RESPONSE;
}

void e104_bt01_direct_uart_get_default_config(
    e104_bt01_direct_uart_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (e104_bt01_direct_uart_config_t) {
        .baud_rate = E104_BT01_DEFAULT_BAUD_RATE,
    };
}

esp_err_t e104_bt01_direct_uart_init(
    const e104_bt01_direct_uart_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(config->baud_rate > 0, ESP_ERR_INVALID_ARG, TAG, "invalid baud rate");

    if (s_initialized) {
        return s_active_config.baud_rate == config->baud_rate
                   ? ESP_OK
                   : ESP_ERR_INVALID_STATE;
    }

    bsp_uart_config_t uart_config = {0};
    bsp_uart_get_default_config(&uart_config);
    uart_config.baud_rate = config->baud_rate;

    ESP_RETURN_ON_ERROR(bsp_uart_init(&uart_config), TAG, "bsp_uart_init failed");

    s_active_config = *config;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t e104_bt01_direct_uart_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_uart_deinit(), TAG, "bsp_uart_deinit failed");
    s_initialized = false;
    memset(&s_active_config, 0, sizeof(s_active_config));
    return ESP_OK;
}

esp_err_t e104_bt01_direct_uart_write(const void *data,
                                      size_t length,
                                      uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "driver is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL || length == 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "data is NULL");
    return bsp_uart_write(data, length, timeout_ms);
}

int e104_bt01_direct_uart_read(void *data,
                               size_t capacity,
                               uint32_t timeout_ms)
{
    if (!s_initialized || data == NULL || capacity == 0U) {
        return -1;
    }
    return bsp_uart_read(data, capacity, timeout_ms);
}

esp_err_t e104_bt01_direct_uart_at_transaction(
    const char *command,
    char *response,
    size_t response_capacity,
    e104_bt01_response_t *result,
    uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "driver is not initialized");
    ESP_RETURN_ON_FALSE(command != NULL && command[0] != '\0',
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "command is empty");
    ESP_RETURN_ON_FALSE(strchr(command, '\r') == NULL && strchr(command, '\n') == NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "command contains CR or LF");
    ESP_RETURN_ON_FALSE(response != NULL && response_capacity >= 2U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "response buffer is invalid");
    ESP_RETURN_ON_FALSE(result != NULL, ESP_ERR_INVALID_ARG, TAG, "result is NULL");
    ESP_RETURN_ON_FALSE(timeout_ms > 0U, ESP_ERR_INVALID_ARG, TAG, "timeout is zero");

    response[0] = '\0';
    *result = (e104_bt01_response_t) {0};

    int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000);

    ESP_RETURN_ON_ERROR(bsp_uart_flush_input(), TAG, "failed to flush stale input");

    uint32_t remaining_ms = e104_bt01_remaining_ms(deadline_us);
    ESP_RETURN_ON_FALSE(remaining_ms > 0U, ESP_ERR_TIMEOUT, TAG, "AT transaction timed out");
    ESP_RETURN_ON_ERROR(bsp_uart_write(command, strlen(command), remaining_ms),
                        TAG,
                        "failed to send AT command");

    size_t response_length = 0U;
    while (true) {
        remaining_ms = e104_bt01_remaining_ms(deadline_us);
        if (remaining_ms == 0U) {
            return ESP_ERR_TIMEOUT;
        }

        uint8_t byte = 0U;
        int read_length = bsp_uart_read(&byte, 1U, remaining_ms);
        if (read_length < 0) {
            return ESP_FAIL;
        }
        if (read_length == 0) {
            return ESP_ERR_TIMEOUT;
        }
        if (response_length + 1U >= response_capacity) {
            response[response_length] = '\0';
            return ESP_ERR_INVALID_SIZE;
        }

        response[response_length++] = (char)byte;
        response[response_length] = '\0';

        if (e104_bt01_has_crlf_suffix(response, response_length)) {
            return e104_bt01_parse_response(response, response_length, result);
        }
    }
}
