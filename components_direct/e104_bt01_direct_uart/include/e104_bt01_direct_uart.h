#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define E104_BT01_DEFAULT_BAUD_RATE       19200
#define E104_BT01_BLE_PAYLOAD_LENGTH      20U

typedef struct {
    int baud_rate;
} e104_bt01_direct_uart_config_t;

typedef enum {
    E104_BT01_RESPONSE_INVALID = 0,
    E104_BT01_RESPONSE_OK,
    E104_BT01_RESPONSE_OK_WITH_VALUE,
    E104_BT01_RESPONSE_MODULE_ERROR,
} e104_bt01_response_type_t;

typedef struct {
    e104_bt01_response_type_t type;
    int module_error;
    size_t length;
} e104_bt01_response_t;

void e104_bt01_direct_uart_get_default_config(
    e104_bt01_direct_uart_config_t *config);

esp_err_t e104_bt01_direct_uart_init(
    const e104_bt01_direct_uart_config_t *config);

esp_err_t e104_bt01_direct_uart_deinit(void);

esp_err_t e104_bt01_direct_uart_write(
    const void *data,
    size_t length,
    uint32_t timeout_ms);

int e104_bt01_direct_uart_read(
    void *data,
    size_t capacity,
    uint32_t timeout_ms);

esp_err_t e104_bt01_direct_uart_at_transaction(
    const char *command,
    char *response,
    size_t response_capacity,
    e104_bt01_response_t *result,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
