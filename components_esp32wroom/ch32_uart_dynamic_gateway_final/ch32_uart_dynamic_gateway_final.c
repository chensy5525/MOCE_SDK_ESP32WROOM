#include "ch32_uart_dynamic_gateway_final.h"

#include "ch32_i2c_multi_gateway_final.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define DYN_CMD_REQUEST_ID              0xF0U
#define DYN_CMD_ASSIGN_ID               0xF1U
#define DYN_CMD_ID_ACK                  0xF2U
#define DYN_MAGIC_AA                    0xAAU
#define DYN_MAGIC_55                    0x55U

#define CAN_ID_DISCOVERY                0x000U
#define CAN_ID_START_BASE               0x200U
#define CAN_ID_DATA_BASE                0x300U
#define CAN_ID_UART_RX_BASE             0x400U
#define CAN_ID_ACK_BASE                 0x500U
#define CAN_ID_HELLO_BASE               0x700U

#define UART_NODE_ID_MIN                0x31U
#define UART_NODE_ID_MAX                0x40U
#define UART_FRAME_MAX_LENGTH           4096U
#define UART_DATA_PAYLOAD_MAX           5U
#define DISCOVERY_QUERY_INTERVAL_MS     500U
#define DISCOVERY_SETTLE_MS             100U
#define ACK_PHASE_START                 0x30U
#define ACK_PHASE_COMPLETE              0x31U

static const char *TAG = "CH32_UART_DYNAMIC";
static bool s_initialized;
static ch32_uart_dynamic_config_t s_config;
static uint8_t s_query_sequence;
static uint8_t s_transfer_id;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static uint16_t get_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static void put_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    size_t index;
    uint8_t bit;

    for (index = 0U; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? (uint16_t)((crc << 1U) ^ 0x1021U)
                      : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static ch32_uart_dynamic_node_t *find_by_token(
    ch32_uart_dynamic_node_t *nodes, size_t count, uint16_t token)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (nodes[index].token == token) {
            return &nodes[index];
        }
    }
    return NULL;
}

static bool node_id_used(const ch32_uart_dynamic_node_t *nodes,
                         size_t count, uint8_t node_id,
                         const ch32_uart_dynamic_node_t *except)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (&nodes[index] != except && nodes[index].node_id == node_id) {
            return true;
        }
    }
    return false;
}

static uint8_t allocate_node_id(const ch32_uart_dynamic_node_t *nodes,
                                size_t count,
                                const ch32_uart_dynamic_node_t *except)
{
    uint8_t candidate;
    for (candidate = UART_NODE_ID_MIN; candidate <= UART_NODE_ID_MAX;
         ++candidate) {
        if (!node_id_used(nodes, count, candidate, except)) {
            return candidate;
        }
    }
    return 0U;
}

static ch32_uart_dynamic_result_t send_discovery_query(void)
{
    uint8_t data[8] = {
        DYN_CMD_REQUEST_ID,
        CH32_UART_DYNAMIC_DEVICE_TYPE,
        CH32_UART_DYNAMIC_PROTOCOL_VERSION,
        0U, ++s_query_sequence, 0U, 0U, 0U
    };
    return ch32_can_gateway_send_frame(CAN_ID_DISCOVERY, data, sizeof(data)) ==
                   CH32_I2C_MULTI_RESULT_OK
               ? CH32_UART_DYNAMIC_OK
               : CH32_UART_DYNAMIC_COMM_FAIL;
}

static ch32_uart_dynamic_result_t assign_node(
    ch32_uart_dynamic_node_t *node, uint8_t node_id)
{
    uint8_t data[8] = {
        DYN_CMD_ASSIGN_ID, node_id, DYN_MAGIC_AA, DYN_MAGIC_55,
        (uint8_t)node->token, (uint8_t)(node->token >> 8U),
        CH32_UART_DYNAMIC_DEVICE_TYPE,
        CH32_UART_DYNAMIC_PROTOCOL_VERSION
    };

    node->node_id = node_id;
    node->ready = false;
    return ch32_can_gateway_send_frame(CAN_ID_DISCOVERY, data, sizeof(data)) ==
                   CH32_I2C_MULTI_RESULT_OK
               ? CH32_UART_DYNAMIC_OK
               : CH32_UART_DYNAMIC_COMM_FAIL;
}

static void drain_observer_queue(uint32_t settle_ms)
{
    ch32_i2c_multi_can_frame_t frame;
    uint32_t deadline = now_ms() + settle_ms;
    do {
        uint32_t wait_ms = settle_ms == 0U ? 0U : 5U;
        (void)ch32_can_gateway_poll_observed_frame(&frame, wait_ms);
    } while (settle_ms != 0U && (int32_t)(deadline - now_ms()) > 0);
}

static ch32_uart_dynamic_result_t wait_transfer_ack(
    const ch32_uart_dynamic_node_t *node, uint8_t transfer_id,
    uint8_t expected_phase, uint32_t timeout_ms, uint16_t *processed_len)
{
    uint32_t deadline = now_ms() + timeout_ms;

    while ((int32_t)(deadline - now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        ch32_i2c_multi_result_t result = ch32_can_gateway_poll_control_frame(
            CH32_UART_DYNAMIC_DEVICE_TYPE, &frame, 10U);
        uint16_t source_id;

        if (result != CH32_I2C_MULTI_RESULT_OK) {
            continue;
        }
        if (frame.id != CAN_ID_ACK_BASE + node->node_id || frame.dlc != 8U ||
            frame.data[0] != expected_phase ||
            frame.data[4] != transfer_id || frame.data[5] != 0U) {
            continue;
        }
        source_id = get_u16_le(&frame.data[1]);
        if (source_id != (expected_phase == ACK_PHASE_START
                              ? CAN_ID_START_BASE + node->node_id
                              : CAN_ID_DATA_BASE + node->node_id)) {
            continue;
        }
        *processed_len = get_u16_le(&frame.data[6]);
        return frame.data[3] == 1U ? CH32_UART_DYNAMIC_OK
                                   : CH32_UART_DYNAMIC_PROTOCOL_ERROR;
    }
    return CH32_UART_DYNAMIC_TIMEOUT;
}

void ch32_uart_dynamic_default_config(ch32_uart_dynamic_config_t *config)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->discovery_window_ms = 3000U;
    config->assign_ack_timeout_ms = 500U;
    config->transfer_ack_timeout_ms = 1000U;
}

int ch32_uart_dynamic_init(const ch32_uart_dynamic_config_t *config)
{
    if (config == NULL || config->discovery_window_ms == 0U ||
        config->assign_ack_timeout_ms == 0U ||
        config->transfer_ack_timeout_ms == 0U) {
        return -1;
    }
    s_config = *config;
    s_initialized = true;
    return 0;
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_discover_incremental(
    ch32_uart_dynamic_node_t *nodes, uint8_t max_nodes, size_t *count)
{
    uint32_t deadline;
    uint32_t next_query_ms;
    bool confirmed = false;
    bool transmit_failed = false;
    size_t index;

    if (!s_initialized || nodes == NULL || count == NULL || max_nodes == 0U ||
        *count > max_nodes) {
        return CH32_UART_DYNAMIC_COMM_FAIL;
    }
    drain_observer_queue(0U);
    if (send_discovery_query() != CH32_UART_DYNAMIC_OK) {
        transmit_failed = true;
    }
    deadline = now_ms() + s_config.discovery_window_ms;
    next_query_ms = now_ms() + DISCOVERY_QUERY_INTERVAL_MS;

    while ((int32_t)(deadline - now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        ch32_uart_dynamic_node_t *node;
        uint16_t token;
        uint32_t current_ms = now_ms();

        if ((int32_t)(current_ms - next_query_ms) >= 0) {
            if (send_discovery_query() != CH32_UART_DYNAMIC_OK) {
                transmit_failed = true;
            }
            next_query_ms = current_ms + DISCOVERY_QUERY_INTERVAL_MS;
        }
        if (ch32_can_gateway_poll_observed_frame(&frame, 50U) !=
            CH32_I2C_MULTI_RESULT_OK) {
            continue;
        }
        if (frame.id >= CAN_ID_HELLO_BASE + UART_NODE_ID_MIN &&
            frame.id <= CAN_ID_HELLO_BASE + UART_NODE_ID_MAX &&
            frame.dlc >= 4U &&
            frame.data[0] == CH32_UART_DYNAMIC_DEVICE_TYPE) {
            uint8_t node_id = (uint8_t)(frame.id - CAN_ID_HELLO_BASE);
            for (index = 0U; index < *count; ++index) {
                if (nodes[index].node_id == node_id) {
                    nodes[index].capabilities = frame.data[3];
                    nodes[index].ready = true;
                    nodes[index].recovered_from_hello = true;
                    confirmed = true;
                    break;
                }
            }
            continue;
        }
        if (frame.id != CAN_ID_DISCOVERY || frame.dlc != 8U) {
            continue;
        }
        if (frame.data[0] == DYN_CMD_ID_ACK &&
            frame.data[2] == DYN_MAGIC_AA &&
            frame.data[3] == DYN_MAGIC_55 &&
            frame.data[6] == CH32_UART_DYNAMIC_DEVICE_TYPE &&
            frame.data[7] == CH32_UART_DYNAMIC_PROTOCOL_VERSION) {
            uint8_t node_id = frame.data[1];
            token = get_u16_le(&frame.data[4]);
            if (token == 0U || node_id < UART_NODE_ID_MIN ||
                node_id > UART_NODE_ID_MAX) {
                continue;
            }
            node = find_by_token(nodes, *count, token);
            if (node == NULL && *count < max_nodes) {
                node = &nodes[(*count)++];
                memset(node, 0, sizeof(*node));
                node->token = token;
            }
            if (node == NULL) {
                continue;
            }
            if (node_id_used(nodes, *count, node_id, node)) {
                uint8_t replacement = allocate_node_id(nodes, *count, node);
                if (replacement != 0U &&
                    assign_node(node, replacement) != CH32_UART_DYNAMIC_OK) {
                    transmit_failed = true;
                }
                continue;
            }
            node->node_id = node_id;
            node->protocol_version = frame.data[7];
            node->ready = true;
            node->active_baud = 9600U;
            confirmed = true;
            ESP_LOGI(TAG, "F2 confirmed type=0x%02X token=0x%04X node=%u",
                     CH32_UART_DYNAMIC_DEVICE_TYPE, token, node_id);
            continue;
        }
        if (frame.data[0] == DYN_CMD_REQUEST_ID &&
            frame.data[1] == CH32_UART_DYNAMIC_DEVICE_TYPE &&
            frame.data[2] == CH32_UART_DYNAMIC_PROTOCOL_VERSION) {
            token = get_u16_le(&frame.data[5]);
            if (token == 0U) {
                continue;
            }
            node = find_by_token(nodes, *count, token);
            if (node == NULL) {
                if (*count >= max_nodes) {
                    continue;
                }
                node = &nodes[(*count)++];
                memset(node, 0, sizeof(*node));
                node->token = token;
            }
            node->capabilities = frame.data[3];
            node->protocol_version = frame.data[2];
            if (!node->ready) {
                uint8_t node_id = allocate_node_id(nodes, *count, node);
                if (node_id != 0U &&
                    assign_node(node, node_id) != CH32_UART_DYNAMIC_OK) {
                    transmit_failed = true;
                }
            }
        }
    }
    drain_observer_queue(DISCOVERY_SETTLE_MS);
    if (confirmed) {
        return CH32_UART_DYNAMIC_OK;
    }
    return transmit_failed ? CH32_UART_DYNAMIC_COMM_FAIL
                           : CH32_UART_DYNAMIC_TIMEOUT;
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_send(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t length)
{
    uint8_t start_frame[7] = {0};
    uint16_t processed_len = 0U;
    uint16_t sequence = 0U;
    size_t offset = 0U;
    ch32_uart_dynamic_result_t result;

    if (node == NULL || data == NULL || length == 0U ||
        length > UART_FRAME_MAX_LENGTH) {
        return CH32_UART_DYNAMIC_LENGTH_ERROR;
    }
    if (!node->ready || node->node_id < UART_NODE_ID_MIN ||
        node->node_id > UART_NODE_ID_MAX) {
        return CH32_UART_DYNAMIC_NODE_NOT_READY;
    }
    s_transfer_id++;
    if (s_transfer_id == 0U) {
        s_transfer_id++;
    }
    start_frame[0] = s_transfer_id;
    put_u16_le(&start_frame[1], (uint16_t)length);
    put_u16_le(&start_frame[3], crc16_ccitt_false(data, length));
    start_frame[5] = CH32_UART_DYNAMIC_PROTOCOL_VERSION;
    start_frame[6] = 0U;

    if (ch32_can_gateway_send_frame(CAN_ID_START_BASE + node->node_id,
                                    start_frame, sizeof(start_frame)) !=
        CH32_I2C_MULTI_RESULT_OK) {
        return CH32_UART_DYNAMIC_COMM_FAIL;
    }
    result = wait_transfer_ack(node, s_transfer_id, ACK_PHASE_START,
                               s_config.assign_ack_timeout_ms,
                               &processed_len);
    if (result != CH32_UART_DYNAMIC_OK) {
        return result;
    }

    while (offset < length) {
        uint8_t frame[8] = {0};
        size_t chunk = length - offset;
        if (chunk > UART_DATA_PAYLOAD_MAX) {
            chunk = UART_DATA_PAYLOAD_MAX;
        }
        frame[0] = s_transfer_id;
        put_u16_le(&frame[1], sequence++);
        memcpy(&frame[3], &data[offset], chunk);
        if (ch32_can_gateway_send_frame(CAN_ID_DATA_BASE + node->node_id,
                                        frame, (uint8_t)(3U + chunk)) !=
            CH32_I2C_MULTI_RESULT_OK) {
            return CH32_UART_DYNAMIC_COMM_FAIL;
        }
        offset += chunk;
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    result = wait_transfer_ack(node, s_transfer_id, ACK_PHASE_COMPLETE,
                               s_config.transfer_ack_timeout_ms,
                               &processed_len);
    if (result != CH32_UART_DYNAMIC_OK) {
        return result;
    }
    return processed_len == length ? CH32_UART_DYNAMIC_OK
                                   : CH32_UART_DYNAMIC_LENGTH_ERROR;
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_send_profile(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t length,
    ch32_uart_dynamic_profile_t profile)
{
    if (profile != CH32_UART_DYNAMIC_PROFILE_RX9600_TX9600) {
        return CH32_UART_DYNAMIC_PROTOCOL_ERROR;
    }
    return ch32_uart_dynamic_send(node, data, length);
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_send_tx_9600(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t length)
{
    return ch32_uart_dynamic_send(node, data, length);
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_send_tx_115200(
    ch32_uart_dynamic_node_t *node, const uint8_t *data, size_t length)
{
    (void)node;
    (void)data;
    (void)length;
    return CH32_UART_DYNAMIC_PROTOCOL_ERROR;
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_set_baud(
    ch32_uart_dynamic_node_t *node, ch32_uart_dynamic_baud_t baud)
{
    if (node == NULL || !node->ready) {
        return CH32_UART_DYNAMIC_NODE_NOT_READY;
    }
    return baud == CH32_UART_DYNAMIC_BAUD_9600
               ? CH32_UART_DYNAMIC_OK
               : CH32_UART_DYNAMIC_PROTOCOL_ERROR;
}

ch32_uart_dynamic_result_t ch32_uart_dynamic_poll_rx(
    ch32_uart_dynamic_node_t *nodes, size_t count,
    ch32_uart_dynamic_rx_t *rx, uint32_t timeout_ms)
{
    ch32_i2c_multi_can_frame_t frame;
    size_t index;

    if (nodes == NULL || rx == NULL) {
        return CH32_UART_DYNAMIC_COMM_FAIL;
    }
    memset(rx, 0, sizeof(*rx));
    if (ch32_can_gateway_poll_control_frame(CH32_UART_DYNAMIC_DEVICE_TYPE,
                                            &frame, timeout_ms) !=
        CH32_I2C_MULTI_RESULT_OK) {
        return CH32_UART_DYNAMIC_NO_DATA;
    }
    for (index = 0U; index < count; ++index) {
        if (frame.id == CAN_ID_UART_RX_BASE + nodes[index].node_id &&
            frame.dlc >= 3U) {
            rx->node = &nodes[index];
            rx->len = frame.dlc;
            memcpy(rx->data, frame.data, frame.dlc);
            return CH32_UART_DYNAMIC_OK;
        }
    }
    return CH32_UART_DYNAMIC_NO_DATA;
}

ch32_uart_dynamic_node_t *ch32_uart_dynamic_find_by_id(
    ch32_uart_dynamic_node_t *nodes, size_t count, uint8_t node_id)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (nodes[index].node_id == node_id) {
            return &nodes[index];
        }
    }
    return NULL;
}

ch32_uart_dynamic_node_t *ch32_uart_dynamic_find_by_token(
    ch32_uart_dynamic_node_t *nodes, size_t count, uint16_t token)
{
    return find_by_token(nodes, count, token);
}

const char *ch32_uart_dynamic_result_text(ch32_uart_dynamic_result_t result)
{
    switch (result) {
    case CH32_UART_DYNAMIC_OK: return "OK";
    case CH32_UART_DYNAMIC_TIMEOUT: return "TIMEOUT";
    case CH32_UART_DYNAMIC_COMM_FAIL: return "COMM_FAIL";
    case CH32_UART_DYNAMIC_NO_DATA: return "NO_DATA";
    case CH32_UART_DYNAMIC_OVERFLOW: return "OVERFLOW";
    case CH32_UART_DYNAMIC_PROTOCOL_ERROR: return "PROTOCOL_ERROR";
    case CH32_UART_DYNAMIC_LENGTH_ERROR: return "LENGTH_ERROR";
    case CH32_UART_DYNAMIC_CRC_ERROR: return "CRC_ERROR";
    case CH32_UART_DYNAMIC_NODE_NOT_FOUND: return "NODE_NOT_FOUND";
    case CH32_UART_DYNAMIC_NODE_NOT_READY: return "NODE_NOT_READY";
    case CH32_UART_DYNAMIC_BUSY: return "BUSY";
    default: return "UNKNOWN";
    }
}
