#include "ch32_i2c_multi_gateway_final.h"
#include "ch32_can_gateway_core.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ch32_i2c_multi";

#define DYN_CMD_REQUEST_ID     0xF0U
#define DYN_CMD_ASSIGN_ID      0xF1U
#define DYN_CMD_ID_ACK         0xF2U
#define DYN_MAGIC_AA           0xAAU
#define DYN_MAGIC_55           0x55U
#define NODE_PROTOCOL_VERSION  0x01U

#define CAN_ID_DISCOVERY       0x000U
#define CAN_ID_STATUS_BASE     0x100U
#define CAN_ID_CMD_BASE        0x200U
#define CAN_ID_UART_RX_BASE    0x400U
#define CAN_ID_ACK_BASE        0x500U
#define CAN_ID_HELLO_BASE      0x700U

#define I2C_DEVICE_TYPE        CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C
#define UART_DEVICE_TYPE       0x04U
#define I2C_NODE_ID_MIN        0x01U
#define I2C_NODE_ID_MAX        0x20U

#define I2C_CMD_SCAN           0x01U
#define I2C_CMD_PROBE          0x02U
#define I2C_CMD_WRITE_REG      0x03U
#define I2C_CMD_READ_REGS      0x04U
#define I2C_CMD_WRITE_RAW      0x05U
#define I2C_CMD_WRITE_READ     0x06U
#define I2C_CMD_SET_SPEED      0x07U
#define I2C_CMD_WRITE_MULTI    0x08U

#define I2C_STATUS_SCAN        0x01U
#define I2C_STATUS_PROBE       0x02U
#define I2C_STATUS_WRITE       0x03U
#define I2C_STATUS_READ_CHUNK  0x05U
#define I2C_STATUS_READ_DONE   0x06U
#define I2C_STATUS_RAW_WRITE   0x07U
#define I2C_STATUS_SPEED       0x08U
#define I2C_STATUS_WRITE_MULTI 0x09U

#define I2C_MULTI_FLAG_START   0x01U
#define I2C_MULTI_FLAG_END     0x02U
#define I2C_MULTI_CHUNK_MAX    4U
#define I2C_MULTI_BUFFER_MAX   136U
#define DISCOVERY_QUERY_INTERVAL_MS 500U
#define DISCOVERY_SETTLE_MS         100U

static bool s_initialized;
static ch32_i2c_multi_config_t s_cfg;

static uint32_t ch32_i2c_multi_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

void ch32_i2c_multi_default_config(ch32_i2c_multi_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->discovery_timeout_ms = 5000U;
    cfg->command_timeout_ms = 350U;
}


int ch32_i2c_multi_init(const ch32_i2c_multi_config_t *cfg)
{
    if (cfg == NULL || cfg->command_timeout_ms == 0U) {
        return -1;
    }
    if (s_initialized) {
        return 0;
    }
    if (ch32_can_gateway_core_init() != 0) {
        ESP_LOGE(TAG, "shared CAN core init failed");
        return -1;
    }
    s_cfg = *cfg;
    s_initialized = true;
    ESP_LOGI(TAG, "protocol init OK");
    return 0;
}


static ch32_i2c_multi_result_t ch32_i2c_multi_map_core_result(
    ch32_can_gateway_result_t result)
{
    switch (result) {
    case CH32_CAN_GATEWAY_OK: return CH32_I2C_MULTI_RESULT_OK;
    case CH32_CAN_GATEWAY_TIMEOUT: return CH32_I2C_MULTI_RESULT_TIMEOUT;
    case CH32_CAN_GATEWAY_NO_DATA: return CH32_I2C_MULTI_RESULT_NO_DATA;
    case CH32_CAN_GATEWAY_INVALID_ARG:
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    case CH32_CAN_GATEWAY_BUSY: return CH32_I2C_MULTI_RESULT_BUSY;
    case CH32_CAN_GATEWAY_COMM_FAIL:
    default: return CH32_I2C_MULTI_RESULT_COMM_FAIL;
    }
}

ch32_i2c_multi_result_t ch32_can_gateway_send_frame(
    uint32_t id, const uint8_t *data, uint8_t dlc)
{
    return ch32_i2c_multi_map_core_result(
        ch32_can_gateway_core_send(id, data, dlc));
}

void ch32_can_gateway_set_observer_filter(uint32_t filter_id)
{
    (void)filter_id;
}

ch32_i2c_multi_result_t ch32_can_gateway_poll_control_frame(
    uint8_t device_type, ch32_i2c_multi_can_frame_t *frame,
    uint32_t timeout_ms)
{
    return ch32_i2c_multi_map_core_result(
        ch32_can_gateway_core_poll_control(
            device_type, (ch32_can_gateway_frame_t *)frame, timeout_ms));
}

ch32_i2c_multi_result_t ch32_can_gateway_poll_observed_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_i2c_multi_map_core_result(
        ch32_can_gateway_core_poll_observer(
            I2C_DEVICE_TYPE, (ch32_can_gateway_frame_t *)frame,
            timeout_ms));
}

ch32_i2c_multi_result_t ch32_i2c_multi_poll_can_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_i2c_multi_map_core_result(
        ch32_can_gateway_core_poll_data(
            (ch32_can_gateway_frame_t *)frame, timeout_ms));
}

static void ch32_i2c_multi_drain_observer_queue(uint32_t settle_ms)
{
    ch32_i2c_multi_can_frame_t discarded;
    uint32_t deadline = ch32_i2c_multi_now_ms() + settle_ms;
    ch32_i2c_multi_result_t result;

    do {
        uint32_t wait_ms = settle_ms == 0U ? 0U : 10U;
        result = ch32_can_gateway_poll_observed_frame(&discarded, wait_ms);
    } while (settle_ms == 0U
                 ? result == CH32_I2C_MULTI_RESULT_OK
                 : (int32_t)(deadline - ch32_i2c_multi_now_ms()) > 0);
}

static ch32_i2c_multi_node_t *ch32_i2c_multi_find_token(
    ch32_i2c_multi_node_t *nodes, size_t count, uint16_t token)
{
    for (size_t index = 0U; index < count; ++index) {
        if (nodes[index].device_type == I2C_DEVICE_TYPE &&
            nodes[index].token == token) {
            return &nodes[index];
        }
    }
    return NULL;
}

static bool ch32_i2c_multi_id_used(const ch32_i2c_multi_node_t *nodes,
                                   size_t count, uint8_t node_id,
                                   const ch32_i2c_multi_node_t *except)
{
    for (size_t index = 0U; index < count; ++index) {
        if (&nodes[index] != except && nodes[index].node_id == node_id) {
            return true;
        }
    }
    return false;
}

static uint8_t ch32_i2c_multi_allocate_id(
    const ch32_i2c_multi_node_t *nodes, size_t count,
    const ch32_i2c_multi_node_t *except)
{
    for (uint8_t candidate = I2C_NODE_ID_MIN;
         candidate <= I2C_NODE_ID_MAX; ++candidate) {
        if (!ch32_i2c_multi_id_used(nodes, count, candidate, except)) {
            return candidate;
        }
    }
    return 0U;
}

static ch32_i2c_multi_result_t ch32_i2c_multi_send_assignment(
    ch32_i2c_multi_node_t *node, uint8_t node_id)
{
    uint8_t assignment[8] = {
        DYN_CMD_ASSIGN_ID, node_id, DYN_MAGIC_AA, DYN_MAGIC_55,
        (uint8_t)node->token, (uint8_t)(node->token >> 8U),
        I2C_DEVICE_TYPE, NODE_PROTOCOL_VERSION
    };

    node->node_id = node_id;
    node->ready = false;
    return ch32_can_gateway_send_frame(CAN_ID_DISCOVERY, assignment,
                                       sizeof(assignment));
}

ch32_i2c_multi_result_t ch32_i2c_multi_discover_and_assign(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count,
    uint32_t timeout_ms)
{
    return ch32_i2c_multi_discover_incremental(nodes, max_nodes, count,
                                               timeout_ms);
}

ch32_i2c_multi_result_t ch32_i2c_multi_discover_incremental(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count,
    uint32_t timeout_ms)
{
    static uint8_t query_sequence;
    uint8_t query[8] = {
        DYN_CMD_REQUEST_ID, I2C_DEVICE_TYPE, NODE_PROTOCOL_VERSION, 0U,
        0U, 0U, 0U, 0U
    };
    uint32_t deadline;
    uint32_t next_query_ms;
    bool confirmed = false;
    bool transmit_failed = false;

    if (!s_initialized || nodes == NULL || count == NULL ||
        max_nodes == 0U || *count > max_nodes) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    ch32_i2c_multi_drain_observer_queue(0U);
    query[4] = ++query_sequence;
    if (ch32_can_gateway_send_frame(CAN_ID_DISCOVERY, query, sizeof(query)) !=
        CH32_I2C_MULTI_RESULT_OK) {
        transmit_failed = true;
        ESP_LOGW(TAG, "initial F0 transmit deferred seq=%u", query[4]);
    }

    deadline = ch32_i2c_multi_now_ms() + timeout_ms;
    next_query_ms = ch32_i2c_multi_now_ms() + DISCOVERY_QUERY_INTERVAL_MS;
    while ((int32_t)(deadline - ch32_i2c_multi_now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        ch32_i2c_multi_node_t *node;
        uint16_t token;
        uint32_t now_ms = ch32_i2c_multi_now_ms();

        /* 不同查询序号会改变 CH32 的 token 退避槽。即使两个节点本轮
         * 的 0x000 回包碰撞，下一轮也能获得新的发送时机。 */
        if ((int32_t)(now_ms - next_query_ms) >= 0) {
            query[4] = ++query_sequence;
            if (ch32_can_gateway_send_frame(CAN_ID_DISCOVERY, query,
                                            sizeof(query)) !=
                CH32_I2C_MULTI_RESULT_OK) {
                transmit_failed = true;
                ESP_LOGW(TAG, "F0 retry deferred seq=%u", query[4]);
            }
            next_query_ms = now_ms + DISCOVERY_QUERY_INTERVAL_MS;
        }

        if (ch32_can_gateway_poll_observed_frame(&frame, 50U) !=
            CH32_I2C_MULTI_RESULT_OK) {
            continue;
        }
        if (frame.id >= CAN_ID_HELLO_BASE &&
            frame.id < CAN_ID_HELLO_BASE + 256U && frame.dlc >= 4U &&
            frame.data[0] == I2C_DEVICE_TYPE) {
            uint8_t hello_node_id = (uint8_t)(frame.id - CAN_ID_HELLO_BASE);
            for (size_t index = 0U; index < *count; ++index) {
                if (nodes[index].ready && nodes[index].node_id == hello_node_id) {
                    nodes[index].fw_version = frame.data[2];
                    nodes[index].capability_flags = frame.data[3];
                    break;
                }
            }
            continue;
        }
        if (frame.dlc != 8U) {
            continue;
        }

        if (frame.id == CAN_ID_DISCOVERY &&
            frame.data[0] == DYN_CMD_ID_ACK &&
            frame.data[2] == DYN_MAGIC_AA &&
            frame.data[3] == DYN_MAGIC_55 &&
            frame.data[6] == I2C_DEVICE_TYPE &&
            frame.data[1] >= I2C_NODE_ID_MIN &&
            frame.data[1] <= I2C_NODE_ID_MAX) {
            uint8_t acknowledged_id = frame.data[1];
            token = (uint16_t)frame.data[4] |
                    ((uint16_t)frame.data[5] << 8U);
            if (token == 0U) {
                continue;
            }
            node = ch32_i2c_multi_find_token(nodes, *count, token);
            if (node == NULL) {
                bool collision = ch32_i2c_multi_id_used(
                    nodes, *count, acknowledged_id, NULL);
                if (*count >= max_nodes) {
                    continue;
                }
                node = &nodes[(*count)++];
                memset(node, 0, sizeof(*node));
                node->token = token;
                node->device_type = I2C_DEVICE_TYPE;
                if (collision) {
                    uint8_t replacement = ch32_i2c_multi_allocate_id(
                        nodes, *count, node);
                    if (replacement != 0U) {
                        (void)ch32_i2c_multi_send_assignment(node,
                                                             replacement);
                    }
                    continue;
                }
            } else if (ch32_i2c_multi_id_used(nodes, *count,
                                              acknowledged_id, node)) {
                uint8_t replacement = ch32_i2c_multi_allocate_id(
                    nodes, *count, node);
                if (replacement != 0U) {
                    (void)ch32_i2c_multi_send_assignment(node, replacement);
                }
                continue;
            }
            {
                bool first_confirmation = !node->ready;
                node->node_id = acknowledged_id;
                node->fw_version = frame.data[7];
                node->ready = true;
                confirmed = true;
                if (first_confirmation) {
                    ESP_LOGI(TAG,
                             "F2 confirmed type=0x%02X token=0x%04X node=%u fw=%u",
                             I2C_DEVICE_TYPE, token, acknowledged_id,
                             node->fw_version);
                }
            }
            continue;
        }

        if (frame.id != CAN_ID_DISCOVERY ||
            frame.data[0] != DYN_CMD_REQUEST_ID ||
            frame.data[1] != I2C_DEVICE_TYPE) {
            continue;
        }
        token = (uint16_t)frame.data[5] |
                ((uint16_t)frame.data[6] << 8U);
        if (token == 0U) {
            continue;
        }
        node = ch32_i2c_multi_find_token(nodes, *count, token);
        if (node == NULL) {
            if (*count >= max_nodes) {
                continue;
            }
            node = &nodes[(*count)++];
            memset(node, 0, sizeof(*node));
            node->token = token;
            node->device_type = I2C_DEVICE_TYPE;
            ESP_LOGI(TAG,
                     "F0 discovered type=0x%02X token=0x%04X fw=%u caps=0x%02X",
                     I2C_DEVICE_TYPE, token, frame.data[2], frame.data[3]);
        }
        node->fw_version = frame.data[2];
        node->capability_flags = frame.data[3];
        if (node->ready) {
            continue;
        }
        {
            uint8_t assigned_id = ch32_i2c_multi_allocate_id(
                nodes, *count, node);
            if (assigned_id != 0U &&
                ch32_i2c_multi_send_assignment(node, assigned_id) !=
                    CH32_I2C_MULTI_RESULT_OK) {
                transmit_failed = true;
                continue;
            }
            if (assigned_id != 0U) {
                ESP_LOGI(TAG,
                         "F1 sent type=0x%02X token=0x%04X node=%u",
                         I2C_DEVICE_TYPE, token, assigned_id);
            }
        }
    }
    ch32_i2c_multi_drain_observer_queue(DISCOVERY_SETTLE_MS);
    if (confirmed) {
        return CH32_I2C_MULTI_RESULT_OK;
    }
    return transmit_failed ? CH32_I2C_MULTI_RESULT_COMM_FAIL
                           : CH32_I2C_MULTI_RESULT_TIMEOUT;
}

static bool ch32_i2c_multi_node_valid(const ch32_i2c_multi_node_t *node)
{
    return node != NULL && node->ready && node->token != 0U &&
           node->node_id >= I2C_NODE_ID_MIN &&
           node->node_id <= I2C_NODE_ID_MAX;
}

static void ch32_i2c_multi_drain_control_queue(void)
{
    ch32_i2c_multi_can_frame_t discarded;
    while (ch32_can_gateway_poll_control_frame(
               I2C_DEVICE_TYPE, &discarded, 0U) ==
           CH32_I2C_MULTI_RESULT_OK) {
    }
}

static ch32_i2c_multi_result_t ch32_i2c_multi_wait_command(
    const ch32_i2c_multi_node_t *node, uint8_t command_type,
    uint8_t status_type, uint8_t expected_addr, uint8_t expected_detail,
    ch32_i2c_multi_can_frame_t *status_frame)
{
    uint32_t deadline = ch32_i2c_multi_now_ms() + s_cfg.command_timeout_ms;
    bool status_received = false;
    bool status_success = false;
    bool ack_received = false;
    bool ack_success = false;

    while ((int32_t)(deadline - ch32_i2c_multi_now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        if (ch32_can_gateway_poll_control_frame(
                I2C_DEVICE_TYPE, &frame, 20U) !=
            CH32_I2C_MULTI_RESULT_OK) {
            continue;
        }
        if (frame.id == CAN_ID_STATUS_BASE + node->node_id &&
            frame.dlc == 8U && frame.data[0] == status_type) {
            bool matches = true;
            if (status_type != I2C_STATUS_SCAN &&
                status_type != I2C_STATUS_SPEED) {
                matches = frame.data[1] == expected_addr;
            }
            if (status_type == I2C_STATUS_WRITE_MULTI) {
                matches = matches && frame.data[2] == expected_detail;
            } else if (status_type == I2C_STATUS_SPEED) {
                matches = frame.data[1] == expected_detail;
            }
            if (!matches) {
                continue;
            }
            status_received = true;
            if (status_type == I2C_STATUS_PROBE ||
                status_type == I2C_STATUS_SPEED) {
                status_success = frame.data[2] != 0U;
            } else if (status_type == I2C_STATUS_WRITE ||
                       status_type == I2C_STATUS_RAW_WRITE ||
                       status_type == I2C_STATUS_WRITE_MULTI) {
                status_success = frame.data[4] != 0U;
            } else {
                status_success = true;
            }
            if (status_frame != NULL) {
                *status_frame = frame;
            }
        } else if (frame.id == CAN_ID_ACK_BASE + node->node_id &&
                   frame.dlc == 8U && frame.data[0] == command_type &&
                   frame.data[2] == node->node_id &&
                   frame.data[3] == I2C_DEVICE_TYPE) {
            ack_received = true;
            ack_success = frame.data[1] != 0U;
        }
        if (status_received && ack_received) {
            return status_success && ack_success
                       ? CH32_I2C_MULTI_RESULT_OK
                       : CH32_I2C_MULTI_RESULT_COMM_FAIL;
        }
    }
    return CH32_I2C_MULTI_RESULT_TIMEOUT;
}

static ch32_i2c_multi_result_t ch32_i2c_multi_run_command(
    const ch32_i2c_multi_node_t *node, const uint8_t command[8],
    uint8_t status_type, uint8_t expected_addr, uint8_t expected_detail,
    ch32_i2c_multi_can_frame_t *status_frame)
{
    ch32_i2c_multi_result_t result;

    if (!ch32_i2c_multi_node_valid(node)) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    if (ch32_can_gateway_core_lock(s_cfg.command_timeout_ms) !=
        CH32_CAN_GATEWAY_OK) {
        return CH32_I2C_MULTI_RESULT_BUSY;
    }
    ch32_i2c_multi_drain_control_queue();
    result = ch32_can_gateway_send_frame(CAN_ID_CMD_BASE + node->node_id,
                                         command, 8U);
    if (result == CH32_I2C_MULTI_RESULT_OK) {
        result = ch32_i2c_multi_wait_command(
            node, command[0], status_type, expected_addr, expected_detail,
            status_frame);
    }
    ch32_can_gateway_core_unlock();
    return result;
}

ch32_i2c_multi_result_t ch32_i2c_multi_scan(ch32_i2c_multi_node_t *node)
{
    const uint8_t command[8] = {I2C_CMD_SCAN, 0U, 0U, 0U,
                                0U, 0U, 0U, 0U};
    ch32_i2c_multi_can_frame_t status = {0};
    ch32_i2c_multi_result_t result;
    uint8_t reported;

    if (node == NULL) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    result = ch32_i2c_multi_run_command(node, command, I2C_STATUS_SCAN,
                                        0U, 0U, &status);
    if (result != CH32_I2C_MULTI_RESULT_OK) {
        return result;
    }
    node->i2c_addr_count = 0U;
    reported = status.data[1] > 6U ? 6U : status.data[1];
    for (uint8_t index = 0U; index < reported &&
         node->i2c_addr_count < CH32_I2C_MULTI_MAX_ADDRS_PER_NODE; ++index) {
        node->i2c_addrs[node->i2c_addr_count++] = status.data[2U + index];
    }
    return CH32_I2C_MULTI_RESULT_OK;
}

ch32_i2c_multi_result_t ch32_i2c_multi_probe(
    const ch32_i2c_multi_node_t *node, uint8_t addr, bool *found)
{
    uint8_t command[8] = {I2C_CMD_PROBE, addr, 0U, 0U,
                          0U, 0U, 0U, 0U};
    ch32_i2c_multi_can_frame_t status = {0};
    ch32_i2c_multi_result_t result;

    if (found == NULL || addr > 0x7FU) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    *found = false;
    result = ch32_i2c_multi_run_command(node, command, I2C_STATUS_PROBE,
                                        addr, 0U, &status);
    if (result == CH32_I2C_MULTI_RESULT_OK) {
        *found = status.data[2] != 0U;
    } else if (result == CH32_I2C_MULTI_RESULT_COMM_FAIL) {
        /* 地址探测未命中属于有效事务，不代表 CAN 桥接链路故障。 */
        result = CH32_I2C_MULTI_RESULT_OK;
    }
    return result;
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_reg_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    const uint8_t *data, uint8_t len)
{
    uint8_t command[8] = {I2C_CMD_WRITE_REG, addr, reg, len,
                          0U, 0U, 0U, 0U};

    if (addr > 0x7FU || len > 4U || (len > 0U && data == NULL)) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    if (len > 0U) {
        memcpy(&command[4], data, len);
    }
    return ch32_i2c_multi_run_command(node, command, I2C_STATUS_WRITE,
                                      addr, 0U, NULL);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len)
{
    uint8_t command[8] = {I2C_CMD_WRITE_RAW, addr, len, 0U,
                          0U, 0U, 0U, 0U};

    if (addr > 0x7FU || data == NULL || len == 0U || len > 5U) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    memcpy(&command[3], data, len);
    return ch32_i2c_multi_run_command(node, command, I2C_STATUS_RAW_WRITE,
                                      addr, 0U, NULL);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len)
{
    ch32_i2c_multi_result_t result = CH32_I2C_MULTI_RESULT_OK;
    uint8_t offset = 0U;

    if (!ch32_i2c_multi_node_valid(node) || addr > 0x7FU || data == NULL ||
        len == 0U || len > I2C_MULTI_BUFFER_MAX) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    if (ch32_can_gateway_core_lock(s_cfg.command_timeout_ms) !=
        CH32_CAN_GATEWAY_OK) {
        return CH32_I2C_MULTI_RESULT_BUSY;
    }
    ch32_i2c_multi_drain_control_queue();
    while (offset < len) {
        uint8_t chunk = (uint8_t)(len - offset);
        uint8_t flags = 0U;
        uint8_t command[8] = {I2C_CMD_WRITE_MULTI, addr, 0U, 0U,
                              0U, 0U, 0U, 0U};
        if (chunk > I2C_MULTI_CHUNK_MAX) {
            chunk = I2C_MULTI_CHUNK_MAX;
        }
        if (offset == 0U) {
            flags |= I2C_MULTI_FLAG_START;
        }
        if ((uint16_t)offset + chunk == len) {
            flags |= I2C_MULTI_FLAG_END;
        }
        command[2] = flags;
        command[3] = chunk;
        memcpy(&command[4], &data[offset], chunk);
        result = ch32_can_gateway_send_frame(CAN_ID_CMD_BASE + node->node_id,
                                             command, 8U);
        if (result == CH32_I2C_MULTI_RESULT_OK) {
            result = ch32_i2c_multi_wait_command(
                node, I2C_CMD_WRITE_MULTI, I2C_STATUS_WRITE_MULTI,
                addr, flags, NULL);
        }
        if (result != CH32_I2C_MULTI_RESULT_OK) {
            break;
        }
        offset = (uint8_t)(offset + chunk);
    }
    ch32_can_gateway_core_unlock();
    return result;
}

static uint8_t ch32_i2c_multi_primary_addr(const ch32_i2c_multi_node_t *node)
{
    return node != NULL && node->i2c_addr_count > 0U
               ? node->i2c_addrs[0]
               : 0xFFU;
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_raw(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_write_to(node, ch32_i2c_multi_primary_addr(node),
                                   data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_raw_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_write_raw(node, data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg,
    const uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_write_reg_to(
        node, ch32_i2c_multi_primary_addr(node), reg, data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_from(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    uint8_t *data, uint8_t len)
{
    static uint8_t request_sequence;
    uint8_t request_id = ++request_sequence;
    uint8_t command[8] = {I2C_CMD_READ_REGS, addr, reg, len,
                          request_id, 0U, 0U, 0U};
    uint32_t deadline;
    uint8_t received = 0U;
    bool done_received = false;
    bool done_success = false;
    bool ack_received = false;
    bool ack_success = false;
    ch32_i2c_multi_result_t result;

    if (request_id == 0U) {
        request_id = ++request_sequence;
        command[4] = request_id;
    }
    if (!ch32_i2c_multi_node_valid(node) || addr > 0x7FU || data == NULL ||
        len == 0U || len > 32U) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    if (ch32_can_gateway_core_lock(s_cfg.command_timeout_ms) !=
        CH32_CAN_GATEWAY_OK) {
        return CH32_I2C_MULTI_RESULT_BUSY;
    }
    ch32_i2c_multi_drain_control_queue();
    result = ch32_can_gateway_send_frame(CAN_ID_CMD_BASE + node->node_id,
                                         command, 8U);
    if (result != CH32_I2C_MULTI_RESULT_OK) {
        ch32_can_gateway_core_unlock();
        return result;
    }

    deadline = ch32_i2c_multi_now_ms() + s_cfg.command_timeout_ms;
    while ((int32_t)(deadline - ch32_i2c_multi_now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        if (ch32_can_gateway_poll_control_frame(
                I2C_DEVICE_TYPE, &frame, 20U) !=
            CH32_I2C_MULTI_RESULT_OK) {
            continue;
        }
        if (frame.id == CAN_ID_STATUS_BASE + node->node_id &&
            frame.dlc == 8U && frame.data[0] == I2C_STATUS_READ_CHUNK &&
            frame.data[1] == request_id && frame.data[3] <= 4U &&
            (uint16_t)frame.data[2] + frame.data[3] <= len) {
            memcpy(&data[frame.data[2]], &frame.data[4], frame.data[3]);
            if ((uint8_t)(frame.data[2] + frame.data[3]) > received) {
                received = (uint8_t)(frame.data[2] + frame.data[3]);
            }
        } else if (frame.id == CAN_ID_STATUS_BASE + node->node_id &&
                   frame.dlc == 8U && frame.data[0] == I2C_STATUS_READ_DONE &&
                   frame.data[1] == request_id && frame.data[2] == addr &&
                   frame.data[3] == reg && frame.data[4] == len) {
            done_received = true;
            done_success = frame.data[5] != 0U && frame.data[6] == len;
        } else if (frame.id == CAN_ID_ACK_BASE + node->node_id &&
                   frame.dlc == 8U && frame.data[0] == I2C_CMD_READ_REGS &&
                   frame.data[2] == node->node_id &&
                   frame.data[3] == I2C_DEVICE_TYPE) {
            ack_received = true;
            ack_success = frame.data[1] != 0U;
        }
        if (done_received && ack_received) {
            result = done_success && ack_success && received == len
                         ? CH32_I2C_MULTI_RESULT_OK
                         : CH32_I2C_MULTI_RESULT_COMM_FAIL;
            ch32_can_gateway_core_unlock();
            return result;
        }
    }
    ch32_can_gateway_core_unlock();
    return CH32_I2C_MULTI_RESULT_TIMEOUT;
}

ch32_i2c_multi_result_t ch32_i2c_multi_read_regs(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_read_regs_from(
        node, ch32_i2c_multi_primary_addr(node), reg, data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_once(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_read_regs(node, reg, data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_multi(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_write_multi_to(
        node, ch32_i2c_multi_primary_addr(node), data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len)
{
    return ch32_i2c_multi_write_multi(node, data, len);
}

ch32_i2c_multi_result_t ch32_i2c_multi_write_read_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg,
    const uint8_t *wdata, uint8_t wlen, uint8_t *rdata, uint8_t rlen)
{
    if (wlen != 1U || wdata == NULL || wdata[0] != reg) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    return ch32_i2c_multi_read_regs_from(
        node, ch32_i2c_multi_primary_addr(node), reg, rdata, rlen);
}

static ch32_i2c_multi_result_t ch32_i2c_multi_set_speed(
    ch32_i2c_multi_node_t *node, uint8_t speed_code)
{
    uint8_t command[8] = {I2C_CMD_SET_SPEED, speed_code, 0U, 0U,
                          0U, 0U, 0U, 0U};
    return ch32_i2c_multi_run_command(node, command, I2C_STATUS_SPEED,
                                      0U, speed_code, NULL);
}

ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_100k(
    ch32_i2c_multi_node_t *node)
{
    return ch32_i2c_multi_set_speed(node, 0U);
}

ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_400k(
    ch32_i2c_multi_node_t *node)
{
    return ch32_i2c_multi_set_speed(node, 1U);
}

uint32_t ch32_i2c_multi_get_status(ch32_i2c_multi_status_field_t field)
{
    return ch32_can_gateway_core_get_status(
        (ch32_can_gateway_status_field_t)field);
}

const char *ch32_i2c_multi_result_text(ch32_i2c_multi_result_t result)
{
    switch (result) {
    case CH32_I2C_MULTI_RESULT_OK:
        return "OK";
    case CH32_I2C_MULTI_RESULT_TIMEOUT:
        return "TIMEOUT";
    case CH32_I2C_MULTI_RESULT_COMM_FAIL:
        return "COMM_FAIL";
    case CH32_I2C_MULTI_RESULT_NO_DATA:
        return "NO_DATA";
    case CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND:
        return "NODE_NOT_FOUND";
    case CH32_I2C_MULTI_RESULT_INVALID_ARG:
        return "INVALID_ARG";
    case CH32_I2C_MULTI_RESULT_BUSY:
        return "BUSY";
    default:
        return "UNKNOWN";
    }
}

const char *ch32_i2c_multi_role_text(ch32_i2c_multi_role_t role)
{
    return role == CH32_I2C_MULTI_ROLE_MASTER ? "MASTER" : "SLAVE";
}
