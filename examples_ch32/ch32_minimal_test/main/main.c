#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "ch32_node_minimal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_BITRATE_TEXT       "50 kbit/s"
#define CAN_RX_QUEUE_LEN       32
#define CAN_TX_QUEUE_DEPTH     4
#define MAIN_LOOP_WAIT_MS      50
#define STATUS_PRINT_PERIOD_MS 3000

static const char *TAG = "ch32_minimal_test";

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} can_rx_item_t;

typedef struct {
    bool online;
    ch32_node_minimal_hello_t hello;
    TickType_t last_seen_tick;
} ch32_node_state_t;

static QueueHandle_t s_rx_queue;
static ch32_node_state_t s_nodes[CH32_NODE_MINIMAL_NODE_ID_MAX + 1U];

static bool IRAM_ATTR on_rx_done(twai_node_handle_t handle,
                                 const twai_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)edata;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    BaseType_t woken = pdFALSE;
    can_rx_item_t item = {0};
    twai_frame_t frame = {
        .buffer = item.data,
        .buffer_len = sizeof(item.data),
    };

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        item.header = frame.header;
        (void)xQueueSendFromISR(queue, &item, &woken);
    }

    return woken == pdTRUE;
}

static bool IRAM_ATTR on_error(twai_node_handle_t handle,
                               const twai_error_event_data_t *edata,
                               void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGW(TAG, "TWAI bus error flags=0x%" PRIx32, edata->err_flags.val);
    return false;
}

static bool IRAM_ATTR on_state_change(twai_node_handle_t handle,
                                      const twai_state_change_event_data_t *edata,
                                      void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGI(TAG, "TWAI state %d -> %d", edata->old_sta, edata->new_sta);
    return false;
}

static twai_node_handle_t start_can_node(void)
{
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = BOARD_CAN_TX_GPIO,
            .rx = BOARD_CAN_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = 50000,
            .sp_permill = 800,
        },
        .timestamp_resolution_hz = 1000000,
        .fail_retry_cnt = 3,
        .tx_queue_depth = CAN_TX_QUEUE_DEPTH,
    };

    twai_node_handle_t node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

    twai_mask_filter_config_t accept_all_standard = {
        .id = 0,
        .mask = 0,
        .is_ext = false,
        .no_fd = true,
    };
    ESP_ERROR_CHECK(twai_node_config_mask_filter(node, 0, &accept_all_standard));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = on_rx_done,
        .on_error = on_error,
        .on_state_change = on_state_change,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, s_rx_queue));
    ESP_ERROR_CHECK(twai_node_enable(node));
    return node;
}

static void process_rx_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr) {
        return;
    }

    ch32_node_minimal_hello_t hello = {0};
    if (ch32_node_minimal_parse_hello(item->header.id, item->data, item->header.dlc, &hello)) {
        bool first_seen = !s_nodes[hello.node_id].online;
        s_nodes[hello.node_id].online = true;
        s_nodes[hello.node_id].hello = hello;
        s_nodes[hello.node_id].last_seen_tick = xTaskGetTickCount();

        if (first_seen) {
            ESP_LOGI(TAG, "HELLO node=%u type=%s fw=%u cap=0x%02X",
                     hello.node_id,
                     ch32_node_minimal_device_type_text(hello.device_type),
                     hello.fw_version,
                     hello.capability_flags);
            printf("hello node=%u type=%s fw=%u cap=0x%02X\r\n",
                   hello.node_id,
                   ch32_node_minimal_device_type_text(hello.device_type),
                   hello.fw_version,
                   hello.capability_flags);
        }
        return;
    }

    ch32_node_minimal_ack_t ack = {0};
    if (ch32_node_minimal_parse_ack(item->header.id, item->data, item->header.dlc, &ack)) {
        ESP_LOGI(TAG, "ACK node=%u type=%s cmd=0x%02X result=%u",
                 ack.node_id,
                 ch32_node_minimal_device_type_text(ack.device_type),
                 ack.command_type,
                 ack.result ? 1U : 0U);
    }
}

static void print_node_summary(void)
{
    printf("ch32 nodes:");
    for (uint8_t id = 1U; id <= CH32_NODE_MINIMAL_NODE_ID_MAX; ++id) {
        if (s_nodes[id].online) {
            const ch32_node_minimal_hello_t *hello = &s_nodes[id].hello;
            printf(" [%u %s fw=%u cap=0x%02X]",
                   id,
                   ch32_node_minimal_device_type_text(hello->device_type),
                   hello->fw_version,
                   hello->capability_flags);
        }
    }
    printf("\r\n");
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM ch32_minimal_test");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM ch32_minimal_test ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("listen only: hello=0x700+node ack=0x500+node\r\n\r\n");

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    ESP_ERROR_CHECK(s_rx_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for CH32 HELLO/HEARTBEAT");
    printf("twai started, waiting for CH32 HELLO/HEARTBEAT\r\n");

    TickType_t last_status_tick = 0;
    while (true) {
        can_rx_item_t item = {0};
        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
            process_rx_frame(&item);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_status_tick) >= pdMS_TO_TICKS(STATUS_PRINT_PERIOD_MS)) {
            last_status_tick = now;
            twai_node_status_t status = {0};
            (void)twai_node_get_info(node, &status, NULL);
            ESP_LOGI(TAG, "status state=%d tx_err=%u rx_err=%u",
                     status.state, status.tx_error_count, status.rx_error_count);
            print_node_summary();
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_WAIT_MS));
    }
}
