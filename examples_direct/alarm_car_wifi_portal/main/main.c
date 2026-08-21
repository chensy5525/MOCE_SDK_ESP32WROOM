#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_alarm_portal.h"

#define ALARM_SCHEDULE_ID                 0U
#define ALARM_COMMAND_QUEUE_LENGTH        4U
#define ALARM_SCHEDULER_TASK_PRIORITY     5U
#define ALARM_CHECK_PERIOD_MS             1000U
#define ALARM_STATE_LOCK_TIMEOUT_MS       50U
#define MIN_VALID_EPOCH_SECONDS           INT64_C(1577836800)
#define MIN_UTC_OFFSET_MINUTES            (-720)
#define MAX_UTC_OFFSET_MINUTES            840
#define VALID_WEEKDAY_MASK                UINT8_C(0x7F)
#define INVALID_LOCAL_MINUTE_KEY          INT64_MIN

typedef enum {
    ALARM_COMMAND_APPLY_CONFIG = 0,
    ALARM_COMMAND_SYNC_TIME,
    ALARM_COMMAND_STOP_CURRENT_RINGING,
} AlarmCommandType;

typedef struct {
    AlarmCommandType type;

    union {
        WifiAlarmConfig config;

        struct {
            int64_t epoch_seconds;
            int16_t utc_offset_minutes;
        } time_sync;
    } data;
} AlarmCommand;

typedef struct {
    WifiAlarmConfig config;
    int16_t utc_offset_minutes;
    int64_t last_triggered_local_minute;
    bool clock_synced;
    bool ringing;
} AlarmSchedulerState;

static const char *TAG = "alarm_car";

static QueueHandle_t s_command_queue;
static SemaphoreHandle_t s_state_mutex;
static AlarmSchedulerState s_state;

/*
 * Product integration may override this weak symbol and enqueue work for a motor
 * or audio controller. The override must be bounded and non-blocking, and must
 * not call back into the portal synchronously.
 */
__attribute__((weak)) void alarm_car_on_alarm_triggered(uint8_t schedule_id)
{
    (void)schedule_id;
}

static bool alarm_config_is_valid(const WifiAlarmConfig *config)
{
    if (config == NULL) {
        return false;
    }

    if (config->schedule_id != ALARM_SCHEDULE_ID ||
        config->hour > 23U ||
        config->minute > 59U) {
        return false;
    }

    if ((config->weekday_mask & (uint8_t)(~VALID_WEEKDAY_MASK)) != 0U) {
        return false;
    }

    if (config->enabled && config->weekday_mask == 0U) {
        return false;
    }

    return true;
}

static void log_alarm_config(const WifiAlarmConfig *config)
{
    ESP_LOGI(TAG,
             "ALARM_CONFIG_APPLIED: schedule_id=%u time=%02u:%02u "
             "weekday_mask=0x%02X repeat=%s enabled=%s",
             (unsigned int)config->schedule_id,
             (unsigned int)config->hour,
             (unsigned int)config->minute,
             (unsigned int)config->weekday_mask,
             config->repeat ? "true" : "false",
             config->enabled ? "true" : "false");
}

static esp_err_t enqueue_alarm_command(const AlarmCommand *command)
{
    if (command == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_command_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(s_command_queue, command, 0U) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static esp_err_t portal_event_callback(const WifiAlarmPortalEvent *event,
                                       void *user_context)
{
    AlarmCommand command = {0};

    (void)user_context;

    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case WIFI_ALARM_PORTAL_EVENT_CONFIG_UPDATED:
        if (!alarm_config_is_valid(&event->data.config)) {
            return ESP_ERR_INVALID_ARG;
        }

        command.type = ALARM_COMMAND_APPLY_CONFIG;
        command.data.config = event->data.config;
        break;

    case WIFI_ALARM_PORTAL_EVENT_TIME_SYNC:
        if (event->data.time_sync.epoch_seconds < MIN_VALID_EPOCH_SECONDS ||
            event->data.time_sync.utc_offset_minutes < MIN_UTC_OFFSET_MINUTES ||
            event->data.time_sync.utc_offset_minutes > MAX_UTC_OFFSET_MINUTES) {
            return ESP_ERR_INVALID_ARG;
        }

        command.type = ALARM_COMMAND_SYNC_TIME;
        command.data.time_sync.epoch_seconds =
            event->data.time_sync.epoch_seconds;
        command.data.time_sync.utc_offset_minutes =
            event->data.time_sync.utc_offset_minutes;
        break;

    case WIFI_ALARM_PORTAL_EVENT_STOP_CURRENT_RINGING:
        command.type = ALARM_COMMAND_STOP_CURRENT_RINGING;
        break;

    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return enqueue_alarm_command(&command);
}

static esp_err_t portal_status_callback(WifiAlarmRuntimeStatus *status,
                                        void *user_context)
{
    time_t epoch_seconds;

    (void)user_context;

    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_state_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_state_mutex,
                       pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    status->clock_synced = s_state.clock_synced;
    status->ringing = s_state.ringing;
    status->utc_offset_minutes = s_state.utc_offset_minutes;

    xSemaphoreGive(s_state_mutex);

    epoch_seconds = status->clock_synced ? time(NULL) : 0;
    status->epoch_seconds = (int64_t)epoch_seconds;
    return ESP_OK;
}

static esp_err_t apply_time_sync(int64_t epoch_seconds,
                                 int16_t utc_offset_minutes)
{
    const struct timeval time_value = {
        .tv_sec = (time_t)epoch_seconds,
        .tv_usec = 0,
    };

    if (settimeofday(&time_value, NULL) != 0) {
        ESP_LOGE(TAG, "TIME_SYNC failed: settimeofday rejected value");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(s_state_mutex,
                       pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_state.utc_offset_minutes = utc_offset_minutes;
    s_state.clock_synced = true;

    xSemaphoreGive(s_state_mutex);

    ESP_LOGI(TAG,
             "TIME_SYNCED: epoch_seconds=%" PRId64 " utc_offset_minutes=%d",
             epoch_seconds,
             (int)utc_offset_minutes);
    return ESP_OK;
}

static uint8_t weekday_to_mask_bit(int tm_wday)
{
    /* struct tm uses Sunday=0; the portal contract uses bit0=Monday. */
    return (uint8_t)(UINT8_C(1) << ((tm_wday + 6) % 7));
}

static bool get_local_time(struct tm *local_time,
                           int64_t *local_minute_key)
{
    int16_t utc_offset_minutes;
    bool clock_synced;
    time_t epoch_seconds;
    time_t shifted_seconds;

    if (local_time == NULL || local_minute_key == NULL) {
        return false;
    }

    if (xSemaphoreTake(s_state_mutex,
                       pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    clock_synced = s_state.clock_synced;
    utc_offset_minutes = s_state.utc_offset_minutes;

    xSemaphoreGive(s_state_mutex);

    if (!clock_synced) {
        return false;
    }

    epoch_seconds = time(NULL);
    if ((int64_t)epoch_seconds < MIN_VALID_EPOCH_SECONDS) {
        return false;
    }

    shifted_seconds = epoch_seconds + ((time_t)utc_offset_minutes * 60);
    if (gmtime_r(&shifted_seconds, local_time) == NULL) {
        return false;
    }

    *local_minute_key = (int64_t)shifted_seconds / 60;
    return true;
}

static void persist_one_shot_disable(const WifiAlarmConfig *triggered_config)
{
    WifiAlarmConfig disabled_config = *triggered_config;
    esp_err_t err;

    disabled_config.enabled = false;

    /* Never hold s_state_mutex while calling a Portal API. */
    err = wifi_alarm_portal_set_config(&disabled_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "one-shot persistence failed: %s",
                 esp_err_to_name(err));
        return;
    }

    if (xSemaphoreTake(s_state_mutex,
                       pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) == pdTRUE) {
        s_state.config.enabled = false;
        xSemaphoreGive(s_state_mutex);
    } else {
        /* The queued CONFIG_UPDATED event will retry the local state update. */
        ESP_LOGW(TAG, "one-shot persisted; local update deferred");
    }

    ESP_LOGI(TAG, "one-shot alarm disabled and persisted");
}

static void check_alarm_trigger(void)
{
    WifiAlarmConfig config_snapshot;
    struct tm local_time = {0};
    int64_t local_minute_key;
    uint8_t current_weekday_bit;
    bool should_trigger = false;

    if (!get_local_time(&local_time, &local_minute_key)) {
        return;
    }

    if (xSemaphoreTake(s_state_mutex,
                       pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return;
    }

    config_snapshot = s_state.config;
    current_weekday_bit = weekday_to_mask_bit(local_time.tm_wday);

    if (config_snapshot.enabled &&
        (config_snapshot.weekday_mask & current_weekday_bit) != 0U &&
        config_snapshot.hour == (uint8_t)local_time.tm_hour &&
        config_snapshot.minute == (uint8_t)local_time.tm_min &&
        s_state.last_triggered_local_minute != local_minute_key) {
        s_state.last_triggered_local_minute = local_minute_key;
        s_state.ringing = true;
        should_trigger = true;
    }

    xSemaphoreGive(s_state_mutex);

    if (!should_trigger) {
        return;
    }

    ESP_LOGI(TAG,
             "ALARM_TRIGGERED: schedule_id=%u local=%04d-%02d-%02d "
             "%02d:%02d:%02d repeat=%s",
             (unsigned int)config_snapshot.schedule_id,
             local_time.tm_year + 1900,
             local_time.tm_mon + 1,
             local_time.tm_mday,
             local_time.tm_hour,
             local_time.tm_min,
             local_time.tm_sec,
             config_snapshot.repeat ? "true" : "false");

    alarm_car_on_alarm_triggered(config_snapshot.schedule_id);

    if (!config_snapshot.repeat) {
        persist_one_shot_disable(&config_snapshot);
    }
}

static void process_alarm_command(const AlarmCommand *command)
{
    esp_err_t err;

    switch (command->type) {
    case ALARM_COMMAND_APPLY_CONFIG:
        if (!alarm_config_is_valid(&command->data.config)) {
            ESP_LOGE(TAG, "queued alarm configuration is invalid");
            break;
        }

        if (xSemaphoreTake(s_state_mutex,
                           pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "alarm config apply failed: state lock timeout");
            break;
        }

        s_state.config = command->data.config;
        xSemaphoreGive(s_state_mutex);
        log_alarm_config(&command->data.config);
        break;

    case ALARM_COMMAND_SYNC_TIME:
        err = apply_time_sync(command->data.time_sync.epoch_seconds,
                              command->data.time_sync.utc_offset_minutes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "time sync apply failed: %s", esp_err_to_name(err));
        }
        break;

    case ALARM_COMMAND_STOP_CURRENT_RINGING:
        if (xSemaphoreTake(s_state_mutex,
                           pdMS_TO_TICKS(ALARM_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "stop ringing failed: state lock timeout");
            break;
        }

        s_state.ringing = false;
        xSemaphoreGive(s_state_mutex);
        ESP_LOGI(TAG,
                 "ALARM_RINGING_STOPPED: schedule_id=%u",
                 ALARM_SCHEDULE_ID);
        break;

    default:
        ESP_LOGE(TAG, "unsupported alarm command: %d", (int)command->type);
        break;
    }
}

static void alarm_scheduler_task(void *argument)
{
    TickType_t next_wake = xTaskGetTickCount();
    AlarmCommand command;

    (void)argument;

    for (;;) {
        while (xQueueReceive(s_command_queue, &command, 0U) == pdPASS) {
            process_alarm_command(&command);
        }

        check_alarm_trigger();
        vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(ALARM_CHECK_PERIOD_MS));
    }
}

static void cleanup_startup_failure(bool portal_started)
{
    esp_err_t err;

    if (portal_started) {
        err = wifi_alarm_portal_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "portal cleanup failed: %s",
                     esp_err_to_name(err));
        }
    }

    if (s_command_queue != NULL) {
        vQueueDelete(s_command_queue);
        s_command_queue = NULL;
    }

    if (s_state_mutex != NULL) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
    }
}

void app_main(void)
{
    const WifiAlarmPortalOptions options = {
        .ap_ssid = CONFIG_ALARM_CAR_PORTAL_AP_SSID,
        .ap_password = CONFIG_ALARM_CAR_PORTAL_AP_PASSWORD,
        .max_connections = CONFIG_ALARM_CAR_PORTAL_MAX_CONNECTIONS,
        .apply_callback = NULL,
        .event_callback = portal_event_callback,
        .status_callback = portal_status_callback,
        .user_context = NULL,
    };
    WifiAlarmConfig restored_config = {0};
    BaseType_t task_result;
    esp_err_t err;

    s_state_mutex = xSemaphoreCreateMutex();
    s_command_queue = xQueueCreate(ALARM_COMMAND_QUEUE_LENGTH,
                                   sizeof(AlarmCommand));
    if (s_state_mutex == NULL || s_command_queue == NULL) {
        ESP_LOGE(TAG, "failed to create alarm scheduler RTOS resources");
        cleanup_startup_failure(false);
        return;
    }

    s_state.last_triggered_local_minute = INVALID_LOCAL_MINUTE_KEY;
    s_state.clock_synced = false;
    s_state.ringing = false;

    err = wifi_alarm_portal_start(&options);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "portal start failed: %s", esp_err_to_name(err));
        cleanup_startup_failure(false);
        return;
    }

    err = wifi_alarm_portal_get_config(&restored_config);
    if (err != ESP_OK || !alarm_config_is_valid(&restored_config)) {
        ESP_LOGE(TAG,
                 "restored config rejected: %s",
                 err == ESP_OK ? "invalid fields" : esp_err_to_name(err));
        cleanup_startup_failure(true);
        return;
    }

    s_state.config = restored_config;

    task_result = xTaskCreate(alarm_scheduler_task,
                              "alarm_scheduler",
                              CONFIG_ALARM_CAR_SCHEDULER_TASK_STACK_SIZE,
                              NULL,
                              ALARM_SCHEDULER_TASK_PRIORITY,
                              NULL);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "failed to create alarm scheduler task");
        cleanup_startup_failure(true);
        return;
    }

    ESP_LOGI(TAG,
             "alarm portal ready: ssid=%s url=http://192.168.4.1",
             options.ap_ssid);
    ESP_LOGI(TAG, "waiting for phone TIME_SYNC before alarm scheduling");
}
