#include "ch32_i2c_multi_gateway_final.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "driver/twai.h"
#pragma GCC diagnostic pop

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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
static SemaphoreHandle_t s_can_mutex;
static QueueHandle_t s_rx_queue;
static QueueHandle_t s_ctl_queue_i2c;
static QueueHandle_t s_ctl_queue_uart;
static QueueHandle_t s_obs_queue;
static volatile uint32_t s_isr_rx_count;
static volatile uint32_t s_route_data_count;
static volatile uint32_t s_route_i2c_count;
static volatile uint32_t s_route_uart_count;
static volatile uint32_t s_route_observer_count;
static volatile uint32_t s_app_rx_count;
static volatile uint32_t s_tx_count;

static uint32_t ch32_i2c_multi_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static void ch32_can_route_frame(const twai_message_t *message)
{
    ch32_i2c_multi_can_frame_t frame = {0};

    frame.id = message->identifier;
    frame.dlc = message->data_length_code > 8U ? 8U : message->data_length_code;
    frame.extd = (message->flags & TWAI_MSG_FLAG_EXTD) != 0U;
    memcpy(frame.data, message->data, frame.dlc);

    s_isr_rx_count++;
    if (frame.id == CAN_ID_DISCOVERY ||
        (frame.id >= CAN_ID_HELLO_BASE &&
         frame.id < CAN_ID_HELLO_BASE + 256U)) {
        if (xQueueSend(s_obs_queue, &frame, 0U) == pdTRUE) {
            s_route_observer_count++;
        }
    } else if (frame.id >= CAN_ID_UART_RX_BASE &&
               frame.id < CAN_ID_UART_RX_BASE + 256U) {
        if (xQueueSend(s_ctl_queue_uart, &frame, 0U) == pdTRUE) {
            s_route_uart_count++;
        }
    } else if ((frame.id >= CAN_ID_STATUS_BASE &&
                frame.id < CAN_ID_STATUS_BASE + 256U) ||
               (frame.id >= CAN_ID_ACK_BASE &&
                frame.id < CAN_ID_ACK_BASE + 256U)) {
        if (xQueueSend(s_ctl_queue_i2c, &frame, 0U) == pdTRUE) {
            s_route_i2c_count++;
        }
        /* 状态/ACK ID 区间由多种网关共享。此处复制到各控制队列，
         * 保持全局只有一个 TWAI 接收者，再由各协议校验 device_type。 */
        if (xQueueSend(s_ctl_queue_uart, &frame, 0U) == pdTRUE) {
            s_route_uart_count++;
        }
    } else if (xQueueSend(s_rx_queue, &frame, 0U) == pdTRUE) {
        s_route_data_count++;
    }
}

static void ch32_can_rx_task(void *arg)
{
    (void)arg;
    for (;;) {
        twai_message_t message;
        if (twai_receive(&message, portMAX_DELAY) == ESP_OK) {
            ch32_can_route_frame(&message);
        }
    }
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
    twai_general_config_t general_config;
    twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t error;

    if (cfg == NULL || cfg->command_timeout_ms == 0U) {
        return -1;
    }
    if (s_initialized) {
        return 0;
    }
    s_cfg = *cfg;
    general_config = (twai_general_config_t)TWAI_GENERAL_CONFIG_DEFAULT(
        BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO, TWAI_MODE_NORMAL);

    error = twai_driver_install(&general_config, &timing_config, &filter_config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(error));
        return -1;
    }
    error = twai_start();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(error));
        (void)twai_driver_uninstall();
        return -1;
    }

    s_rx_queue = xQueueCreate(32U, sizeof(ch32_i2c_multi_can_frame_t));
    s_ctl_queue_i2c = xQueueCreate(64U, sizeof(ch32_i2c_multi_can_frame_t));
    s_ctl_queue_uart = xQueueCreate(64U, sizeof(ch32_i2c_multi_can_frame_t));
    s_obs_queue = xQueueCreate(64U, sizeof(ch32_i2c_multi_can_frame_t));
    s_can_mutex = xSemaphoreCreateMutex();
    if (s_rx_queue == NULL || s_ctl_queue_i2c == NULL ||
        s_ctl_queue_uart == NULL || s_obs_queue == NULL || s_can_mutex == NULL) {
        ESP_LOGE(TAG, "CAN routing resource allocation failed");
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return -1;
    }
    if (xTaskCreate(ch32_can_rx_task, "ch32_can_rx", 4096U, NULL, 10U,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "CAN RX routing task creation failed");
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return -1;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "init OK bitrate=%u tx=%d rx=%d",
             (unsigned)CH32_CAN_GATEWAY_BITRATE_HZ,
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    return 0;
}

ch32_i2c_multi_result_t ch32_can_gateway_send_frame(
    uint32_t id, const uint8_t *data, uint8_t dlc)
{
    twai_message_t message = {0};
    twai_status_info_t status = {0};
    esp_err_t transmit_error;
    static uint32_t last_fault_log_ms;

    if (!s_initialized) {
        return CH32_I2C_MULTI_RESULT_COMM_FAIL;
    }
    if (data == NULL || dlc > 8U || id > 0x7FFU) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    message.identifier = id;
    message.data_length_code = dlc;
    memcpy(message.data, data, dlc);

    transmit_error = twai_transmit(&message, pdMS_TO_TICKS(50U));
    if (transmit_error == ESP_OK) {
        s_tx_count++;
        return CH32_I2C_MULTI_RESULT_OK;
    }

    if (twai_get_status_info(&status) == ESP_OK) {
        uint32_t now_ms = ch32_i2c_multi_now_ms();

        if (status.state == TWAI_STATE_BUS_OFF) {
            esp_err_t recovery_error = twai_initiate_recovery();
            ESP_LOGE(TAG,
                     "CAN bus-off id=0x%03lX tec=%lu rec=%lu bus_err=%lu recovery=%s",
                     (unsigned long)id,
                     (unsigned long)status.tx_error_counter,
                     (unsigned long)status.rx_error_counter,
                     (unsigned long)status.bus_error_count,
                     esp_err_to_name(recovery_error));
            last_fault_log_ms = now_ms;
        } else if (status.state == TWAI_STATE_STOPPED) {
            esp_err_t start_error = twai_start();
            if (start_error == ESP_OK) {
                ESP_LOGW(TAG, "CAN recovery complete, controller restarted");
                transmit_error = twai_transmit(&message, pdMS_TO_TICKS(50U));
                if (transmit_error == ESP_OK) {
                    s_tx_count++;
                    return CH32_I2C_MULTI_RESULT_OK;
                }
            } else {
                ESP_LOGE(TAG, "CAN restart failed: %s",
                         esp_err_to_name(start_error));
            }
            last_fault_log_ms = now_ms;
        } else if ((uint32_t)(now_ms - last_fault_log_ms) >= 1000U) {
            ESP_LOGW(TAG,
                     "CAN TX failed id=0x%03lX err=%s state=%u queued=%lu tec=%lu rec=%lu bus_err=%lu",
                     (unsigned long)id, esp_err_to_name(transmit_error),
                     (unsigned)status.state,
                     (unsigned long)status.msgs_to_tx,
                     (unsigned long)status.tx_error_counter,
                     (unsigned long)status.rx_error_counter,
                     (unsigned long)status.bus_error_count);
            last_fault_log_ms = now_ms;
        }
    }

    if (transmit_error != ESP_OK) {
        return CH32_I2C_MULTI_RESULT_COMM_FAIL;
    }
    return CH32_I2C_MULTI_RESULT_COMM_FAIL;
}

void ch32_can_gateway_set_observer_filter(uint32_t filter_id)
{
    (void)filter_id;
}

static ch32_i2c_multi_result_t ch32_i2c_multi_poll_queue(
    QueueHandle_t queue, ch32_i2c_multi_can_frame_t *frame,
    uint32_t timeout_ms)
{
    TickType_t ticks;

    if (queue == NULL || frame == NULL) {
        return CH32_I2C_MULTI_RESULT_INVALID_ARG;
    }
    ticks = timeout_ms > 0U ? pdMS_TO_TICKS(timeout_ms) : 0U;
    if (xQueueReceive(queue, frame, ticks) != pdTRUE) {
        return CH32_I2C_MULTI_RESULT_NO_DATA;
    }
    s_app_rx_count++;
    return CH32_I2C_MULTI_RESULT_OK;
}

ch32_i2c_multi_result_t ch32_can_gateway_poll_control_frame(
    uint8_t device_type, ch32_i2c_multi_can_frame_t *frame,
    uint32_t timeout_ms)
{
    QueueHandle_t queue = device_type == UART_DEVICE_TYPE
                              ? s_ctl_queue_uart
                              : s_ctl_queue_i2c;
    return ch32_i2c_multi_poll_queue(queue, frame, timeout_ms);
}

ch32_i2c_multi_result_t ch32_can_gateway_poll_observed_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_i2c_multi_poll_queue(s_obs_queue, frame, timeout_ms);
}

ch32_i2c_multi_result_t ch32_i2c_multi_poll_can_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_i2c_multi_poll_queue(s_rx_queue, frame, timeout_ms);
}

static void ch32_i2c_multi_drain_observer_queue(uint32_t settle_ms)
{
    ch32_i2c_multi_can_frame_t discarded;
    uint32_t deadline = ch32_i2c_multi_now_ms() + settle_ms;

    do {
        uint32_t wait_ms = settle_ms == 0U ? 0U : 10U;
        (void)ch32_i2c_multi_poll_queue(s_obs_queue, &discarded, wait_ms);
    } while (settle_ms == 0U
                 ? uxQueueMessagesWaiting(s_obs_queue) > 0U
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

        if (ch32_i2c_multi_poll_queue(s_obs_queue, &frame, 50U) !=
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
    while (ch32_i2c_multi_poll_queue(s_ctl_queue_i2c, &discarded, 0U) ==
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
        if (ch32_i2c_multi_poll_queue(s_ctl_queue_i2c, &frame, 20U) !=
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
    if (xSemaphoreTake(s_can_mutex, pdMS_TO_TICKS(s_cfg.command_timeout_ms)) !=
        pdTRUE) {
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
    xSemaphoreGive(s_can_mutex);
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
    if (xSemaphoreTake(s_can_mutex, pdMS_TO_TICKS(s_cfg.command_timeout_ms)) !=
        pdTRUE) {
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
    xSemaphoreGive(s_can_mutex);
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
    if (xSemaphoreTake(s_can_mutex, pdMS_TO_TICKS(s_cfg.command_timeout_ms)) !=
        pdTRUE) {
        return CH32_I2C_MULTI_RESULT_BUSY;
    }
    ch32_i2c_multi_drain_control_queue();
    result = ch32_can_gateway_send_frame(CAN_ID_CMD_BASE + node->node_id,
                                         command, 8U);
    if (result != CH32_I2C_MULTI_RESULT_OK) {
        xSemaphoreGive(s_can_mutex);
        return result;
    }

    deadline = ch32_i2c_multi_now_ms() + s_cfg.command_timeout_ms;
    while ((int32_t)(deadline - ch32_i2c_multi_now_ms()) > 0) {
        ch32_i2c_multi_can_frame_t frame;
        if (ch32_i2c_multi_poll_queue(s_ctl_queue_i2c, &frame, 20U) !=
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
            xSemaphoreGive(s_can_mutex);
            return result;
        }
    }
    xSemaphoreGive(s_can_mutex);
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
    switch (field) {
    case CH32_I2C_MULTI_STATUS_ISR_RX:
        return s_isr_rx_count;
    case CH32_I2C_MULTI_STATUS_ROUTE_DATA:
        return s_route_data_count;
    case CH32_I2C_MULTI_STATUS_ROUTE_I2C:
        return s_route_i2c_count;
    case CH32_I2C_MULTI_STATUS_ROUTE_UART:
        return s_route_uart_count;
    case CH32_I2C_MULTI_STATUS_ROUTE_OBSERVER:
        return s_route_observer_count;
    case CH32_I2C_MULTI_STATUS_APP_RX:
        return s_app_rx_count;
    case CH32_I2C_MULTI_STATUS_TX:
        return s_tx_count;
    default:
        return 0U;
    }
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
