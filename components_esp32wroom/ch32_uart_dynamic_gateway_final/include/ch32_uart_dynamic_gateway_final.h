#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_UART_DYNAMIC_MAX_NODES              6U
#define CH32_UART_DYNAMIC_CAP_TX_FRAGMENT        0x02U
#define CH32_UART_DYNAMIC_CAP_RX_FORWARD         0x04U
#define CH32_UART_DYNAMIC_CAP_BAUD_CONFIG        0x10U
#define CH32_UART_DYNAMIC_CAP_DYNAMIC_NODE       0x40U
#define CH32_UART_DYNAMIC_PROTOCOL_VERSION       1U
#define CH32_UART_DYNAMIC_DEVICE_TYPE            0x04U

typedef enum {
    CH32_UART_DYNAMIC_OK = 0,
    CH32_UART_DYNAMIC_TIMEOUT,
    CH32_UART_DYNAMIC_COMM_FAIL,
    CH32_UART_DYNAMIC_NO_DATA,
    CH32_UART_DYNAMIC_OVERFLOW,
    CH32_UART_DYNAMIC_PROTOCOL_ERROR,
    CH32_UART_DYNAMIC_LENGTH_ERROR,
    CH32_UART_DYNAMIC_CRC_ERROR,
    CH32_UART_DYNAMIC_NODE_NOT_FOUND,
    CH32_UART_DYNAMIC_NODE_NOT_READY,
    CH32_UART_DYNAMIC_BUSY,
} ch32_uart_dynamic_result_t;

typedef enum {
    CH32_UART_DYNAMIC_BAUD_9600 = 0,
    CH32_UART_DYNAMIC_BAUD_115200 = 1,
} ch32_uart_dynamic_baud_t;

typedef enum {
    CH32_UART_DYNAMIC_PROFILE_RX9600_TX9600 = 0,
    CH32_UART_DYNAMIC_PROFILE_RX115200_TX9600 = 1,
    CH32_UART_DYNAMIC_PROFILE_RX9600_TX115200 = 2,
} ch32_uart_dynamic_profile_t;

typedef struct {
    uint16_t token;
    uint8_t  node_id;
    uint8_t  capabilities;
    uint8_t  protocol_version;
    bool     ready;
    bool     recovered_from_hello;
    uint32_t active_baud;
    uint8_t  _reserved[12];
} ch32_uart_dynamic_node_t;

typedef struct {
    ch32_uart_dynamic_node_t *node;
    uint8_t  data[256];
    uint8_t  len;
} ch32_uart_dynamic_rx_t;

typedef struct {
    uint32_t discovery_window_ms;
    uint32_t assign_ack_timeout_ms;
    uint32_t transfer_ack_timeout_ms;
    uint16_t discovery_query_interval_ms;
    uint16_t discovery_quiet_ms;
    uint8_t  discovery_min_rounds;
    uint8_t  _reserved[3];
} ch32_uart_dynamic_config_t;

void ch32_uart_dynamic_default_config(ch32_uart_dynamic_config_t *cfg);
int  ch32_uart_dynamic_init(const ch32_uart_dynamic_config_t *cfg);

ch32_uart_dynamic_result_t ch32_uart_dynamic_discover_incremental(
    ch32_uart_dynamic_node_t *nodes, uint8_t max_nodes, size_t *count);

ch32_uart_dynamic_result_t ch32_uart_dynamic_send(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t len);
ch32_uart_dynamic_result_t ch32_uart_dynamic_send_profile(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t len,
    ch32_uart_dynamic_profile_t profile);
ch32_uart_dynamic_result_t ch32_uart_dynamic_send_tx_9600(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t len);
ch32_uart_dynamic_result_t ch32_uart_dynamic_send_tx_115200(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t len);

ch32_uart_dynamic_result_t ch32_uart_dynamic_set_baud(
    ch32_uart_dynamic_node_t *node, ch32_uart_dynamic_baud_t baud);

ch32_uart_dynamic_result_t ch32_uart_dynamic_poll_rx(
    ch32_uart_dynamic_node_t *nodes, size_t count,
    ch32_uart_dynamic_rx_t *rx, uint32_t timeout_ms);

ch32_uart_dynamic_node_t *ch32_uart_dynamic_find_by_id(
    ch32_uart_dynamic_node_t *nodes, size_t count, uint8_t node_id);
ch32_uart_dynamic_node_t *ch32_uart_dynamic_find_by_token(
    ch32_uart_dynamic_node_t *nodes, size_t count, uint16_t token);

const char *ch32_uart_dynamic_result_text(ch32_uart_dynamic_result_t result);

#ifdef __cplusplus
}
#endif
