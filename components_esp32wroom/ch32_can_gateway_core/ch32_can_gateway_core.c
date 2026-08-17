#include "ch32_can_gateway_core.h"

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

#define CH32_CAN_ID_DISCOVERY       0x000U
#define CH32_CAN_ID_STATUS_BASE     0x100U
#define CH32_CAN_ID_UART_RX_BASE    0x400U
#define CH32_CAN_ID_ACK_BASE        0x500U
#define CH32_CAN_ID_HELLO_BASE      0x700U
#define CH32_CAN_NODE_ID_MAX        0x4FU
#define CH32_CAN_I2C_SESSION_QUEUE_LEN   68U
#define CH32_CAN_SPI_SESSION_QUEUE_LEN   136U
#define CH32_CAN_UART_SESSION_QUEUE_LEN  16U

typedef struct {
    QueueHandle_t queue;
    SemaphoreHandle_t mutex;
    uint8_t device_type;
    volatile uint32_t overflow_count;
    uint32_t transaction_overflow_snapshot;
} ch32_can_gateway_session_t;

static const char *TAG = "CH32_CAN_CORE";
static bool s_initialized;
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
static SemaphoreHandle_t s_transaction_mutex;
#endif
static SemaphoreHandle_t s_session_registry_mutex;
static QueueHandle_t s_data_queue;
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
static QueueHandle_t s_i2c_control_queue;
static QueueHandle_t s_spi_control_queue;
static QueueHandle_t s_uart_control_queue;
#endif
static QueueHandle_t s_uart_rx_queue;
static QueueHandle_t s_i2c_observer_queue;
static QueueHandle_t s_spi_observer_queue;
static QueueHandle_t s_uart_observer_queue;
static ch32_can_gateway_session_t s_sessions[CH32_CAN_NODE_ID_MAX + 1U];
static volatile uint32_t s_isr_rx_count;
static volatile uint32_t s_route_data_count;
static volatile uint32_t s_route_i2c_count;
static volatile uint32_t s_route_spi_count;
static volatile uint32_t s_route_uart_count;
static volatile uint32_t s_route_observer_count;
static volatile uint32_t s_app_rx_count;
static volatile uint32_t s_tx_count;
static volatile uint32_t s_queue_drop_count;
static portMUX_TYPE s_fault_state_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_recovery_in_progress;
static uint32_t s_last_fault_log_ms;

static void ch32_can_gateway_core_delete_queue(QueueHandle_t *queue)
{
    if (*queue != NULL) {
        vQueueDelete(*queue);
        *queue = NULL;
    }
}

static void ch32_can_gateway_core_delete_mutex(SemaphoreHandle_t *mutex)
{
    if (*mutex != NULL) {
        vSemaphoreDelete(*mutex);
        *mutex = NULL;
    }
}

static void ch32_can_gateway_core_cleanup_routing_resources(void)
{
    ch32_can_gateway_core_delete_queue(&s_data_queue);
    ch32_can_gateway_core_delete_queue(&s_uart_rx_queue);
    ch32_can_gateway_core_delete_queue(&s_i2c_observer_queue);
    ch32_can_gateway_core_delete_queue(&s_spi_observer_queue);
    ch32_can_gateway_core_delete_queue(&s_uart_observer_queue);
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    ch32_can_gateway_core_delete_queue(&s_i2c_control_queue);
    ch32_can_gateway_core_delete_queue(&s_spi_control_queue);
    ch32_can_gateway_core_delete_queue(&s_uart_control_queue);
    ch32_can_gateway_core_delete_mutex(&s_transaction_mutex);
#endif
    ch32_can_gateway_core_delete_mutex(&s_session_registry_mutex);
    for (uint32_t node_id = 0U; node_id <= CH32_CAN_NODE_ID_MAX; ++node_id) {
        ch32_can_gateway_core_delete_queue(&s_sessions[node_id].queue);
        ch32_can_gateway_core_delete_mutex(&s_sessions[node_id].mutex);
        memset(&s_sessions[node_id], 0, sizeof(s_sessions[node_id]));
    }
}

static uint32_t ch32_can_gateway_core_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static bool ch32_can_gateway_core_node_matches(uint8_t device_type,
                                               uint8_t node_id)
{
    if (device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_I2C) {
        return node_id >= 0x01U && node_id <= 0x20U;
    }
    if (device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_SPI) {
        return node_id >= 0x21U && node_id <= 0x30U;
    }
    if (device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_UART) {
        return node_id >= 0x31U && node_id <= CH32_CAN_NODE_ID_MAX;
    }
    return false;
}

static QueueHandle_t ch32_can_gateway_core_observer_queue(uint8_t device_type)
{
    switch (device_type) {
    case CH32_CAN_GATEWAY_DEVICE_TYPE_I2C: return s_i2c_observer_queue;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_SPI: return s_spi_observer_queue;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_UART: return s_uart_observer_queue;
    default: return NULL;
    }
}

#if CH32_CAN_GATEWAY_LEGACY_COMPAT
static QueueHandle_t ch32_can_gateway_core_legacy_control_queue(
    uint8_t device_type)
{
    switch (device_type) {
    case CH32_CAN_GATEWAY_DEVICE_TYPE_I2C: return s_i2c_control_queue;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_SPI: return s_spi_control_queue;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_UART: return s_uart_control_queue;
    default: return NULL;
    }
}
#endif

static uint8_t ch32_can_gateway_core_type_from_node(uint8_t node_id)
{
    if (node_id >= 0x01U && node_id <= 0x20U) {
        return CH32_CAN_GATEWAY_DEVICE_TYPE_I2C;
    }
    if (node_id >= 0x21U && node_id <= 0x30U) {
        return CH32_CAN_GATEWAY_DEVICE_TYPE_SPI;
    }
    if (node_id >= 0x31U && node_id <= CH32_CAN_NODE_ID_MAX) {
        return CH32_CAN_GATEWAY_DEVICE_TYPE_UART;
    }
    return 0U;
}

static UBaseType_t ch32_can_gateway_core_session_queue_len(
    uint8_t device_type)
{
    switch (device_type) {
    case CH32_CAN_GATEWAY_DEVICE_TYPE_I2C:
        return CH32_CAN_I2C_SESSION_QUEUE_LEN;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_SPI:
        return CH32_CAN_SPI_SESSION_QUEUE_LEN;
    case CH32_CAN_GATEWAY_DEVICE_TYPE_UART:
        return CH32_CAN_UART_SESSION_QUEUE_LEN;
    default:
        return 0U;
    }
}

static void ch32_can_gateway_core_route_control(
    const ch32_can_gateway_frame_t *frame, uint8_t node_id,
    uint8_t device_type)
{
    QueueHandle_t session_queue = NULL;
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    QueueHandle_t legacy_queue;
#endif

    if (!ch32_can_gateway_core_node_matches(device_type, node_id)) {
        return;
    }
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    legacy_queue = ch32_can_gateway_core_legacy_control_queue(device_type);
#endif
    if (node_id <= CH32_CAN_NODE_ID_MAX &&
        s_sessions[node_id].device_type == device_type) {
        session_queue = s_sessions[node_id].queue;
    }
    if (session_queue != NULL) {
        if (xQueueSend(session_queue, frame, 0U) != pdTRUE) {
            s_sessions[node_id].overflow_count++;
            s_queue_drop_count++;
        }
    }
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    if (legacy_queue != NULL) {
        (void)xQueueSend(legacy_queue, frame, 0U);
    }
#endif
    if (device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_I2C) {
        s_route_i2c_count++;
    } else if (device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_SPI) {
        s_route_spi_count++;
    } else {
        s_route_uart_count++;
    }
}

static void ch32_can_gateway_core_route(const twai_message_t *message)
{
    ch32_can_gateway_frame_t frame = {0};
    QueueHandle_t observer_queue = NULL;

    frame.id = message->identifier;
    frame.dlc = message->data_length_code > 8U
                    ? 8U : message->data_length_code;
    frame.extd = (message->flags & TWAI_MSG_FLAG_EXTD) != 0U;
    memcpy(frame.data, message->data, frame.dlc);
    s_isr_rx_count++;

    if (frame.id == CH32_CAN_ID_DISCOVERY && frame.dlc == 8U) {
        if (frame.data[0] == 0xF0U) {
            observer_queue = ch32_can_gateway_core_observer_queue(frame.data[1]);
        } else if (frame.data[0] == 0xF2U) {
            observer_queue = ch32_can_gateway_core_observer_queue(frame.data[6]);
        }
        if (observer_queue != NULL) {
            if (xQueueSend(observer_queue, &frame, 0U) == pdTRUE) {
                s_route_observer_count++;
            } else {
                s_queue_drop_count++;
            }
        }
    } else if (frame.id >= CH32_CAN_ID_HELLO_BASE &&
               frame.id < CH32_CAN_ID_HELLO_BASE + 256U &&
               frame.dlc >= 1U) {
        observer_queue = ch32_can_gateway_core_observer_queue(frame.data[0]);
        if (observer_queue != NULL) {
            if (xQueueSend(observer_queue, &frame, 0U) == pdTRUE) {
                s_route_observer_count++;
            } else {
                s_queue_drop_count++;
            }
        }
    } else if (frame.id >= CH32_CAN_ID_UART_RX_BASE &&
               frame.id < CH32_CAN_ID_UART_RX_BASE + 256U) {
        if (xQueueSend(s_uart_rx_queue, &frame, 0U) == pdTRUE) {
            s_route_uart_count++;
        } else {
            s_queue_drop_count++;
        }
    } else if (frame.id >= CH32_CAN_ID_STATUS_BASE &&
               frame.id < CH32_CAN_ID_STATUS_BASE + 256U) {
        uint8_t node_id = (uint8_t)(frame.id - CH32_CAN_ID_STATUS_BASE);
        ch32_can_gateway_core_route_control(
            &frame, node_id, ch32_can_gateway_core_type_from_node(node_id));
    } else if (frame.id >= CH32_CAN_ID_ACK_BASE &&
               frame.id < CH32_CAN_ID_ACK_BASE + 256U &&
               frame.dlc == 8U) {
        uint8_t node_id = (uint8_t)(frame.id - CH32_CAN_ID_ACK_BASE);
        ch32_can_gateway_core_route_control(
            &frame, node_id, ch32_can_gateway_core_type_from_node(node_id));
    } else if (frame.id >= CH32_CAN_ID_ACK_BASE &&
               frame.id < CH32_CAN_ID_ACK_BASE + 256U) {
        uint8_t node_id = (uint8_t)(frame.id - CH32_CAN_ID_ACK_BASE);
        ch32_can_gateway_core_route_control(
            &frame, node_id, ch32_can_gateway_core_type_from_node(node_id));
    } else if (xQueueSend(s_data_queue, &frame, 0U) == pdTRUE) {
        s_route_data_count++;
    } else {
        s_queue_drop_count++;
    }
}

static void ch32_can_gateway_core_rx_task(void *argument)
{
    (void)argument;
    for (;;) {
        twai_message_t message;
        if (twai_receive(&message, portMAX_DELAY) == ESP_OK) {
            ch32_can_gateway_core_route(&message);
        }
    }
}

static ch32_can_gateway_result_t ch32_can_gateway_core_poll_queue(
    QueueHandle_t queue, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    TickType_t ticks;

    if (queue == NULL || frame == NULL) {
        return CH32_CAN_GATEWAY_INVALID_ARG;
    }
    ticks = timeout_ms > 0U ? pdMS_TO_TICKS(timeout_ms) : 0U;
    if (xQueueReceive(queue, frame, ticks) != pdTRUE) {
        return CH32_CAN_GATEWAY_NO_DATA;
    }
    s_app_rx_count++;
    return CH32_CAN_GATEWAY_OK;
}

int ch32_can_gateway_core_init(void)
{
    twai_general_config_t general_config;
    twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t error;

    if (s_initialized) {
        return 0;
    }
    ch32_can_gateway_core_cleanup_routing_resources();
    s_recovery_in_progress = false;
    s_last_fault_log_ms = 0U;
    general_config = (twai_general_config_t)TWAI_GENERAL_CONFIG_DEFAULT(
        BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO, TWAI_MODE_NORMAL);
    error = twai_driver_install(&general_config, &timing_config,
                                &filter_config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s",
                 esp_err_to_name(error));
        return -1;
    }
    error = twai_start();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(error));
        (void)twai_driver_uninstall();
        return -1;
    }

    s_data_queue = xQueueCreate(32U, sizeof(ch32_can_gateway_frame_t));
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    s_i2c_control_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_spi_control_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_uart_control_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
#endif
    s_uart_rx_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_i2c_observer_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_spi_observer_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_uart_observer_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    s_transaction_mutex = xSemaphoreCreateMutex();
#endif
    s_session_registry_mutex = xSemaphoreCreateMutex();
    if (s_data_queue == NULL || s_uart_rx_queue == NULL ||
        s_i2c_observer_queue == NULL ||
        s_spi_observer_queue == NULL || s_uart_observer_queue == NULL ||
        s_session_registry_mutex == NULL
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
        || s_i2c_control_queue == NULL || s_spi_control_queue == NULL ||
        s_uart_control_queue == NULL || s_transaction_mutex == NULL
#endif
        ) {
        ESP_LOGE(TAG, "routing resource allocation failed");
        ch32_can_gateway_core_cleanup_routing_resources();
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return -1;
    }
    if (xTaskCreate(ch32_can_gateway_core_rx_task, "ch32_can_rx", 4096U,
                    NULL, 10U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "CAN RX task creation failed");
        ch32_can_gateway_core_cleanup_routing_resources();
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return -1;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "init OK bitrate=%u tx=%d rx=%d",
             (unsigned)CH32_CAN_GATEWAY_CORE_BITRATE_HZ,
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    return 0;
}

bool ch32_can_gateway_core_is_initialized(void)
{
    return s_initialized;
}

ch32_can_gateway_result_t ch32_can_gateway_core_send(
    uint32_t id, const uint8_t *data, uint8_t dlc)
{
    return ch32_can_gateway_core_send_timeout(id, data, dlc, 50U);
}

ch32_can_gateway_result_t ch32_can_gateway_core_send_timeout(
    uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t timeout_ms)
{
    twai_message_t message = {0};
    twai_status_info_t status = {0};
    esp_err_t transmit_error;

    if (!s_initialized) {
        return CH32_CAN_GATEWAY_COMM_FAIL;
    }
    if (data == NULL || dlc > 8U || id > 0x7FFU) {
        return CH32_CAN_GATEWAY_INVALID_ARG;
    }
    message.identifier = id;
    message.data_length_code = dlc;
    memcpy(message.data, data, dlc);
    transmit_error = twai_transmit(&message, pdMS_TO_TICKS(timeout_ms));
    if (transmit_error == ESP_OK) {
        s_tx_count++;
        return CH32_CAN_GATEWAY_OK;
    }

    if (twai_get_status_info(&status) == ESP_OK) {
        uint32_t current_ms = ch32_can_gateway_core_now_ms();
        if (status.state == TWAI_STATE_BUS_OFF) {
            bool start_recovery = false;
            esp_err_t recovery_error = ESP_OK;

            taskENTER_CRITICAL(&s_fault_state_lock);
            if (!s_recovery_in_progress) {
                s_recovery_in_progress = true;
                start_recovery = true;
            }
            taskEXIT_CRITICAL(&s_fault_state_lock);
            if (start_recovery) {
                recovery_error = twai_initiate_recovery();
                if (recovery_error != ESP_OK) {
                    taskENTER_CRITICAL(&s_fault_state_lock);
                    s_recovery_in_progress = false;
                    taskEXIT_CRITICAL(&s_fault_state_lock);
                }
            }
            if ((start_recovery || s_last_fault_log_ms == 0U) &&
                (s_last_fault_log_ms == 0U ||
                 (uint32_t)(current_ms - s_last_fault_log_ms) >= 1000U)) {
                ESP_LOGE(TAG,
                         "bus-off id=0x%03lX tec=%lu rec=%lu recovery=%s",
                         (unsigned long)id,
                         (unsigned long)status.tx_error_counter,
                         (unsigned long)status.rx_error_counter,
                         start_recovery ? esp_err_to_name(recovery_error)
                                        : "in_progress");
                s_last_fault_log_ms = current_ms;
            }
        } else if (status.state == TWAI_STATE_STOPPED) {
            bool restart_after_recovery = false;

            taskENTER_CRITICAL(&s_fault_state_lock);
            if (s_recovery_in_progress) {
                s_recovery_in_progress = false;
                restart_after_recovery = true;
            }
            taskEXIT_CRITICAL(&s_fault_state_lock);
            if (restart_after_recovery && twai_start() == ESP_OK) {
                ESP_LOGI(TAG, "CAN bus-off recovery complete; driver restarted");
                transmit_error = twai_transmit(&message,
                                               pdMS_TO_TICKS(timeout_ms));
                if (transmit_error == ESP_OK) {
                    s_tx_count++;
                    return CH32_CAN_GATEWAY_OK;
                }
            } else if (restart_after_recovery) {
                taskENTER_CRITICAL(&s_fault_state_lock);
                s_recovery_in_progress = true;
                taskEXIT_CRITICAL(&s_fault_state_lock);
            }
            if (s_last_fault_log_ms == 0U ||
                (uint32_t)(current_ms - s_last_fault_log_ms) >= 1000U) {
                ESP_LOGW(TAG, "CAN recovery waiting/restart failed");
                s_last_fault_log_ms = current_ms;
            }
        } else if (s_last_fault_log_ms == 0U ||
                   (uint32_t)(current_ms - s_last_fault_log_ms) >= 1000U) {
            ESP_LOGW(TAG,
                     "TX failed id=0x%03lX err=%s state=%u tec=%lu rec=%lu",
                     (unsigned long)id, esp_err_to_name(transmit_error),
                     (unsigned)status.state,
                     (unsigned long)status.tx_error_counter,
                     (unsigned long)status.rx_error_counter);
            s_last_fault_log_ms = current_ms;
        }
    }
    return CH32_CAN_GATEWAY_COMM_FAIL;
}

#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_can_gateway_result_t ch32_can_gateway_core_poll_control(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    QueueHandle_t queue = ch32_can_gateway_core_legacy_control_queue(device_type);
    return ch32_can_gateway_core_poll_queue(queue, frame, timeout_ms);
}
#endif

ch32_can_gateway_result_t ch32_can_gateway_core_poll_observer(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    QueueHandle_t queue = ch32_can_gateway_core_observer_queue(device_type);
    return ch32_can_gateway_core_poll_queue(queue, frame, timeout_ms);
}

ch32_can_gateway_result_t ch32_can_gateway_core_poll_data(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_can_gateway_core_poll_queue(s_data_queue, frame, timeout_ms);
}

ch32_can_gateway_result_t ch32_can_gateway_core_session_acquire(
    uint8_t device_type, uint8_t node_id, uint32_t timeout_ms)
{
    ch32_can_gateway_session_t *session;
    uint32_t started_ms;
    uint32_t elapsed_ms;
    uint32_t remaining_ms;

    if (!s_initialized ||
        !ch32_can_gateway_core_node_matches(device_type, node_id) ||
        s_session_registry_mutex == NULL) {
        return CH32_CAN_GATEWAY_INVALID_ARG;
    }
    session = &s_sessions[node_id];
    started_ms = ch32_can_gateway_core_now_ms();
    if (xSemaphoreTake(s_session_registry_mutex,
                       pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return CH32_CAN_GATEWAY_BUSY;
    }
    if (session->queue == NULL) {
        session->queue = xQueueCreate(
            ch32_can_gateway_core_session_queue_len(device_type),
            sizeof(ch32_can_gateway_frame_t));
        session->mutex = xSemaphoreCreateMutex();
        session->device_type = device_type;
    }
    xSemaphoreGive(s_session_registry_mutex);
    if (session->queue == NULL || session->mutex == NULL ||
        session->device_type != device_type) {
        return CH32_CAN_GATEWAY_COMM_FAIL;
    }
    elapsed_ms = ch32_can_gateway_core_now_ms() - started_ms;
    remaining_ms = elapsed_ms < timeout_ms ? timeout_ms - elapsed_ms : 0U;
    return xSemaphoreTake(session->mutex, pdMS_TO_TICKS(remaining_ms)) == pdTRUE
               ? CH32_CAN_GATEWAY_OK : CH32_CAN_GATEWAY_BUSY;
}

ch32_can_gateway_result_t ch32_can_gateway_core_session_poll(
    uint8_t device_type, uint8_t node_id, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    if (!ch32_can_gateway_core_node_matches(device_type, node_id) ||
        s_sessions[node_id].device_type != device_type) {
        return CH32_CAN_GATEWAY_INVALID_ARG;
    }
    ch32_can_gateway_session_t *session = &s_sessions[node_id];
    ch32_can_gateway_result_t result;

    if (session->overflow_count != session->transaction_overflow_snapshot) {
        return CH32_CAN_GATEWAY_OVERFLOW;
    }
    result = ch32_can_gateway_core_poll_queue(session->queue, frame, timeout_ms);
    if (session->overflow_count != session->transaction_overflow_snapshot) {
        return CH32_CAN_GATEWAY_OVERFLOW;
    }
    return result;
}

void ch32_can_gateway_core_session_drain(uint8_t device_type, uint8_t node_id)
{
    ch32_can_gateway_frame_t discarded;

    ch32_can_gateway_session_t *session;

    if (!ch32_can_gateway_core_node_matches(device_type, node_id) ||
        s_sessions[node_id].device_type != device_type) {
        return;
    }
    session = &s_sessions[node_id];
    while (ch32_can_gateway_core_poll_queue(
               session->queue, &discarded, 0U) == CH32_CAN_GATEWAY_OK) {
    }
    session->transaction_overflow_snapshot = session->overflow_count;
}

void ch32_can_gateway_core_session_invalidate(uint8_t device_type,
                                              uint8_t node_id)
{
    ch32_can_gateway_session_t *session;

    if (!ch32_can_gateway_core_node_matches(device_type, node_id) ||
        s_sessions[node_id].device_type != device_type) {
        return;
    }
    session = &s_sessions[node_id];
    session->overflow_count++;
    if (session->queue != NULL) {
        xQueueReset(session->queue);
    }
}

void ch32_can_gateway_core_session_release(uint8_t device_type,
                                           uint8_t node_id)
{
    if (ch32_can_gateway_core_node_matches(device_type, node_id) &&
        s_sessions[node_id].device_type == device_type &&
        s_sessions[node_id].mutex != NULL) {
        xSemaphoreGive(s_sessions[node_id].mutex);
    }
}

ch32_can_gateway_result_t ch32_can_gateway_core_poll_uart_rx(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_can_gateway_core_poll_queue(s_uart_rx_queue, frame, timeout_ms);
}

#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_can_gateway_result_t ch32_can_gateway_core_lock(uint32_t timeout_ms)
{
    if (!s_initialized || s_transaction_mutex == NULL) {
        return CH32_CAN_GATEWAY_COMM_FAIL;
    }
    return xSemaphoreTake(s_transaction_mutex, pdMS_TO_TICKS(timeout_ms)) ==
                   pdTRUE
               ? CH32_CAN_GATEWAY_OK : CH32_CAN_GATEWAY_BUSY;
}
void ch32_can_gateway_core_unlock(void)
{
    if (s_transaction_mutex != NULL) {
        xSemaphoreGive(s_transaction_mutex);
    }
}
#endif

uint32_t ch32_can_gateway_core_get_status(
    ch32_can_gateway_status_field_t field)
{
    switch (field) {
    case CH32_CAN_GATEWAY_STATUS_ISR_RX: return s_isr_rx_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_DATA: return s_route_data_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_I2C: return s_route_i2c_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_SPI: return s_route_spi_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_UART: return s_route_uart_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_OBSERVER:
        return s_route_observer_count;
    case CH32_CAN_GATEWAY_STATUS_APP_RX: return s_app_rx_count;
    case CH32_CAN_GATEWAY_STATUS_TX: return s_tx_count;
    case CH32_CAN_GATEWAY_STATUS_QUEUE_DROPS: return s_queue_drop_count;
    default: return 0U;
    }
}
