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

static const char *TAG = "CH32_CAN_CORE";
static bool s_initialized;
static SemaphoreHandle_t s_transaction_mutex;
static QueueHandle_t s_data_queue;
static QueueHandle_t s_i2c_control_queue;
static QueueHandle_t s_uart_control_queue;
static QueueHandle_t s_i2c_observer_queue;
static QueueHandle_t s_uart_observer_queue;
static volatile uint32_t s_isr_rx_count;
static volatile uint32_t s_route_data_count;
static volatile uint32_t s_route_i2c_count;
static volatile uint32_t s_route_uart_count;
static volatile uint32_t s_route_observer_count;
static volatile uint32_t s_app_rx_count;
static volatile uint32_t s_tx_count;

static uint32_t ch32_can_gateway_core_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
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
            observer_queue = frame.data[1] == CH32_CAN_GATEWAY_DEVICE_TYPE_UART
                                 ? s_uart_observer_queue
                                 : frame.data[1] == CH32_CAN_GATEWAY_DEVICE_TYPE_I2C
                                       ? s_i2c_observer_queue : NULL;
        } else if (frame.data[0] == 0xF2U) {
            observer_queue = frame.data[6] == CH32_CAN_GATEWAY_DEVICE_TYPE_UART
                                 ? s_uart_observer_queue
                                 : frame.data[6] == CH32_CAN_GATEWAY_DEVICE_TYPE_I2C
                                       ? s_i2c_observer_queue : NULL;
        }
        if (observer_queue != NULL &&
            xQueueSend(observer_queue, &frame, 0U) == pdTRUE) {
            s_route_observer_count++;
        }
    } else if (frame.id >= CH32_CAN_ID_HELLO_BASE &&
               frame.id < CH32_CAN_ID_HELLO_BASE + 256U &&
               frame.dlc >= 1U) {
        observer_queue = frame.data[0] == CH32_CAN_GATEWAY_DEVICE_TYPE_UART
                             ? s_uart_observer_queue
                             : frame.data[0] == CH32_CAN_GATEWAY_DEVICE_TYPE_I2C
                                   ? s_i2c_observer_queue : NULL;
        if (observer_queue != NULL &&
            xQueueSend(observer_queue, &frame, 0U) == pdTRUE) {
            s_route_observer_count++;
        }
    } else if (frame.id >= CH32_CAN_ID_UART_RX_BASE &&
               frame.id < CH32_CAN_ID_UART_RX_BASE + 256U) {
        if (xQueueSend(s_uart_control_queue, &frame, 0U) == pdTRUE) {
            s_route_uart_count++;
        }
    } else if (frame.id >= CH32_CAN_ID_STATUS_BASE &&
               frame.id < CH32_CAN_ID_STATUS_BASE + 256U) {
        if (xQueueSend(s_i2c_control_queue, &frame, 0U) == pdTRUE) {
            s_route_i2c_count++;
        }
    } else if (frame.id >= CH32_CAN_ID_ACK_BASE &&
               frame.id < CH32_CAN_ID_ACK_BASE + 256U &&
               frame.dlc == 8U) {
        QueueHandle_t control_queue =
            (frame.data[0] >= 0x30U && frame.data[0] <= 0x34U)
                ? s_uart_control_queue : s_i2c_control_queue;
        if (xQueueSend(control_queue, &frame, 0U) == pdTRUE) {
            if (control_queue == s_uart_control_queue) {
                s_route_uart_count++;
            } else {
                s_route_i2c_count++;
            }
        }
    } else if (frame.id >= CH32_CAN_ID_ACK_BASE &&
               frame.id < CH32_CAN_ID_ACK_BASE + 256U) {
        if (xQueueSend(s_uart_control_queue, &frame, 0U) == pdTRUE) {
            s_route_uart_count++;
        }
    } else if (xQueueSend(s_data_queue, &frame, 0U) == pdTRUE) {
        s_route_data_count++;
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
    s_i2c_control_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_uart_control_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_i2c_observer_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_uart_observer_queue = xQueueCreate(64U, sizeof(ch32_can_gateway_frame_t));
    s_transaction_mutex = xSemaphoreCreateMutex();
    if (s_data_queue == NULL || s_i2c_control_queue == NULL ||
        s_uart_control_queue == NULL || s_i2c_observer_queue == NULL ||
        s_uart_observer_queue == NULL ||
        s_transaction_mutex == NULL) {
        ESP_LOGE(TAG, "routing resource allocation failed");
        (void)twai_stop();
        (void)twai_driver_uninstall();
        return -1;
    }
    if (xTaskCreate(ch32_can_gateway_core_rx_task, "ch32_can_rx", 4096U,
                    NULL, 10U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "CAN RX task creation failed");
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
    twai_message_t message = {0};
    twai_status_info_t status = {0};
    esp_err_t transmit_error;
    static uint32_t last_fault_log_ms;

    if (!s_initialized) {
        return CH32_CAN_GATEWAY_COMM_FAIL;
    }
    if (data == NULL || dlc > 8U || id > 0x7FFU) {
        return CH32_CAN_GATEWAY_INVALID_ARG;
    }
    message.identifier = id;
    message.data_length_code = dlc;
    memcpy(message.data, data, dlc);
    transmit_error = twai_transmit(&message, pdMS_TO_TICKS(50U));
    if (transmit_error == ESP_OK) {
        s_tx_count++;
        return CH32_CAN_GATEWAY_OK;
    }

    if (twai_get_status_info(&status) == ESP_OK) {
        uint32_t current_ms = ch32_can_gateway_core_now_ms();
        if (status.state == TWAI_STATE_BUS_OFF) {
            esp_err_t recovery_error = twai_initiate_recovery();
            ESP_LOGE(TAG,
                     "bus-off id=0x%03lX tec=%lu rec=%lu recovery=%s",
                     (unsigned long)id,
                     (unsigned long)status.tx_error_counter,
                     (unsigned long)status.rx_error_counter,
                     esp_err_to_name(recovery_error));
            last_fault_log_ms = current_ms;
        } else if (status.state == TWAI_STATE_STOPPED) {
            if (twai_start() == ESP_OK) {
                transmit_error = twai_transmit(&message, pdMS_TO_TICKS(50U));
                if (transmit_error == ESP_OK) {
                    s_tx_count++;
                    return CH32_CAN_GATEWAY_OK;
                }
            }
            last_fault_log_ms = current_ms;
        } else if ((uint32_t)(current_ms - last_fault_log_ms) >= 1000U) {
            ESP_LOGW(TAG,
                     "TX failed id=0x%03lX err=%s state=%u tec=%lu rec=%lu",
                     (unsigned long)id, esp_err_to_name(transmit_error),
                     (unsigned)status.state,
                     (unsigned long)status.tx_error_counter,
                     (unsigned long)status.rx_error_counter);
            last_fault_log_ms = current_ms;
        }
    }
    return CH32_CAN_GATEWAY_COMM_FAIL;
}

ch32_can_gateway_result_t ch32_can_gateway_core_poll_control(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    QueueHandle_t queue =
        device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_UART
            ? s_uart_control_queue : s_i2c_control_queue;
    return ch32_can_gateway_core_poll_queue(queue, frame, timeout_ms);
}

ch32_can_gateway_result_t ch32_can_gateway_core_poll_observer(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms)
{
    QueueHandle_t queue =
        device_type == CH32_CAN_GATEWAY_DEVICE_TYPE_UART
            ? s_uart_observer_queue : s_i2c_observer_queue;
    return ch32_can_gateway_core_poll_queue(queue, frame, timeout_ms);
}

ch32_can_gateway_result_t ch32_can_gateway_core_poll_data(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms)
{
    return ch32_can_gateway_core_poll_queue(s_data_queue, frame, timeout_ms);
}

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

uint32_t ch32_can_gateway_core_get_status(
    ch32_can_gateway_status_field_t field)
{
    switch (field) {
    case CH32_CAN_GATEWAY_STATUS_ISR_RX: return s_isr_rx_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_DATA: return s_route_data_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_I2C: return s_route_i2c_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_UART: return s_route_uart_count;
    case CH32_CAN_GATEWAY_STATUS_ROUTE_OBSERVER:
        return s_route_observer_count;
    case CH32_CAN_GATEWAY_STATUS_APP_RX: return s_app_rx_count;
    case CH32_CAN_GATEWAY_STATUS_TX: return s_tx_count;
    default: return 0U;
    }
}
