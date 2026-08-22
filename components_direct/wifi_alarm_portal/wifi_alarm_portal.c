#include "wifi_alarm_portal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portal_dns_server.h"

#define ALARM_NVS_NAMESPACE        "alarm06"
#define ALARM_NVS_KEY              "config"
#define NETWORK_NVS_NAMESPACE      "alarm_net"
#define NETWORK_NVS_KEY            "active"
#define NETWORK_RECORD_MAGIC       0x4E455430UL
#define NETWORK_RECORD_VERSION     2U
#define ALARM_RECORD_MAGIC         0x41303643UL
#define ALARM_RECORD_VERSION_V1    1U
#define ALARM_RECORD_VERSION_V2    2U
#define ALARM_DEFAULT_HOUR         7U
#define ALARM_DEFAULT_MINUTE       0U
#define ALARM_HTTP_BODY_MAX_LEN    384U
#define ALARM_HTTP_RESPONSE_LEN    512U
#define ALARM_MUTEX_TIMEOUT_MS     100U
#define ALARM_HTTP_RECV_MAX_TIMEOUTS 3U
#define ALARM_AP_MAX_CONNECTIONS   4U
#define PORTAL_STA_CONNECTED_BIT   BIT0
#define PORTAL_STA_FAILED_BIT      BIT1
#define PORTAL_SCAN_DONE_BIT       BIT2
#define PORTAL_STA_DISCONNECTED_BIT BIT3
#define NETWORK_PROVISION_TIMEOUT_MS 20000U
#define NETWORK_DISCONNECT_TIMEOUT_MS 2000U
#define NETWORK_PROVISION_TASK_STACK_SIZE 4096U
#define NETWORK_PROVISION_TASK_PRIORITY 4U
#define NETWORK_SCAN_MAX_RESULTS   10U
#define NETWORK_SCAN_RESPONSE_MAX_LEN 3072U
#define ALARM_MIN_EPOCH_SECONDS    INT64_C(1577836800)
#define ALARM_MAX_EPOCH_SECONDS    INT64_C(9007199254740991)
#define ALARM_MIN_UTC_OFFSET_MIN   (-720)
#define ALARM_MAX_UTC_OFFSET_MIN   840
#define PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE "503 Service Unavailable"
#define PORTAL_HTTP_STATUS_ACCEPTED "202 Accepted"
#define PORTAL_HTTP_STATUS_CONFLICT "409 Conflict"

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
    uint8_t reserved[3];
    uint32_t checksum;
} AlarmNvsRecordV1;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t schedule_id;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekday_mask;
    uint8_t repeat;
    uint8_t enabled;
    uint32_t checksum;
} AlarmNvsRecordV2;

_Static_assert(sizeof(AlarmNvsRecordV1) == 16U, "unexpected v1 NVS layout");
_Static_assert(sizeof(AlarmNvsRecordV2) == 16U, "unexpected v2 NVS layout");

typedef union {
    AlarmNvsRecordV1 v1;
    AlarmNvsRecordV2 v2;
    uint8_t bytes[sizeof(AlarmNvsRecordV2)];
} AlarmNvsRecord;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t configured;
    uint8_t reserved;
    char ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U];
    char password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U];
    uint32_t checksum;
} NetworkCredentialRecord;

typedef enum {
    NETWORK_PROVISION_IDLE = 0,
    NETWORK_PROVISION_CONNECTING,
    NETWORK_PROVISION_SUCCESS,
    NETWORK_PROVISION_FAILED,
    NETWORK_PROVISION_CLEARED,
    NETWORK_PROVISION_STORAGE_FAILED,
} NetworkProvisionResult;

typedef struct {
    char candidate_ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U];
    char candidate_password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U];
    char previous_ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U];
    char previous_password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U];
} NetworkProvisionContext;

typedef struct {
    bool running;
    bool wifi_started;
    atomic_bool sta_enabled;
    atomic_bool sta_suspended;
    atomic_bool sta_provisioning;
    atomic_bool scan_running;
    atomic_bool scan_ready;
    esp_netif_t *ap_netif;
    esp_netif_t *sta_netif;
    EventGroupHandle_t sta_event_group;
    esp_event_handler_instance_t wifi_event_handler;
    esp_event_handler_instance_t ip_event_handler;
    atomic_uchar sta_retry_count;
    atomic_ushort sta_disconnect_reason;
    atomic_ushort provision_failure_reason;
    atomic_int provision_result;
    httpd_handle_t http_server;
    PortalDnsServer *dns_server;
    SemaphoreHandle_t mutex;
    WifiAlarmConfig alarm;
    char sta_ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U];
    char sta_password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U];
    WifiAlarmPortalOptions options;
} WifiAlarmPortalState;

static const char *TAG = "wifi_alarm_portal";
static WifiAlarmPortalState s_portal;

static void portal_wifi_event_handler(void *argument,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void *event_data)
{
    (void)argument;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (!atomic_load(&s_portal.sta_enabled) ||
            atomic_load(&s_portal.sta_suspended)) {
            return;
        }
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "station connect start failed: %s",
                     esp_err_to_name(err));
            xEventGroupSetBits(s_portal.sta_event_group,
                               PORTAL_STA_FAILED_BIT);
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = event_data;
        uint16_t reason = (disconnected != NULL) ? disconnected->reason : 0U;

        atomic_store(&s_portal.sta_disconnect_reason, reason);
        xEventGroupClearBits(s_portal.sta_event_group,
                             PORTAL_STA_CONNECTED_BIT);
        xEventGroupSetBits(s_portal.sta_event_group,
                           PORTAL_STA_DISCONNECTED_BIT);
        if (atomic_load(&s_portal.sta_suspended)) {
            return;
        }
        ESP_LOGW(TAG, "station disconnected: reason=%u",
                 (unsigned int)reason);
        unsigned char retry_count = atomic_load(&s_portal.sta_retry_count);
        if (retry_count < s_portal.options.sta_maximum_retries) {
            retry_count = atomic_fetch_add(&s_portal.sta_retry_count, 1U) + 1U;
            esp_err_t err = esp_wifi_connect();
            if (err == ESP_OK) {
                ESP_LOGW(TAG, "station reconnect attempt %u/%u",
                         (unsigned int)retry_count,
                         (unsigned int)s_portal.options.sta_maximum_retries);
                return;
            }
            ESP_LOGE(TAG, "station reconnect request failed: %s",
                     esp_err_to_name(err));
        }
        xEventGroupSetBits(s_portal.sta_event_group, PORTAL_STA_FAILED_BIT);
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        atomic_store(&s_portal.sta_retry_count, 0U);
        atomic_store(&s_portal.sta_disconnect_reason, 0U);
        xEventGroupClearBits(s_portal.sta_event_group,
                             PORTAL_STA_FAILED_BIT);
        xEventGroupSetBits(s_portal.sta_event_group,
                           PORTAL_STA_CONNECTED_BIT);
        ESP_LOGI(TAG, "station obtained an IP address");
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        atomic_store(&s_portal.scan_running, false);
        atomic_store(&s_portal.scan_ready, true);
        xEventGroupSetBits(s_portal.sta_event_group, PORTAL_SCAN_DONE_BIT);
    }
}

extern const uint8_t portal_index_html_start[]
    asm("_binary_index_html_start");
extern const uint8_t portal_index_html_end[]
    asm("_binary_index_html_end");

static uint32_t alarm_v1_checksum(const AlarmNvsRecordV1 *record)
{
    return record->magic ^ record->version ^ record->hour ^ record->minute ^
           record->enabled ^ 0x5A17C3E9UL;
}

static uint32_t alarm_v2_checksum(const AlarmNvsRecordV2 *record)
{
    return record->magic ^ record->version ^ record->schedule_id ^
           record->hour ^ record->minute ^ record->weekday_mask ^
           record->repeat ^ record->enabled ^ 0x5A17C3E9UL;
}

static bool alarm_config_is_valid(const WifiAlarmConfig *config)
{
    return (config != NULL) &&
           (config->schedule_id == 0U) &&
           (config->hour < 24U) &&
           (config->minute < 60U) &&
           ((config->weekday_mask & (uint8_t)~WIFI_ALARM_WEEKDAY_ALL) == 0U) &&
           (!config->enabled || (config->weekday_mask != 0U));
}

static esp_err_t alarm_lock(void)
{
    if ((s_portal.mutex == NULL) ||
        (xSemaphoreTake(s_portal.mutex,
                        pdMS_TO_TICKS(ALARM_MUTEX_TIMEOUT_MS)) != pdTRUE)) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void alarm_unlock(void)
{
    xSemaphoreGive(s_portal.mutex);
}

static void network_secure_zero(void *buffer, size_t size)
{
    volatile uint8_t *bytes = buffer;

    while (size > 0U) {
        *bytes++ = 0U;
        --size;
    }
}

static size_t network_utf8_sequence_length(const uint8_t *text, size_t remaining)
{
    if ((text == NULL) || (remaining == 0U)) {
        return 0U;
    }
    if (text[0] <= 0x7FU) {
        return 1U;
    }
    if ((text[0] >= 0xC2U) && (text[0] <= 0xDFU) &&
        (remaining >= 2U) && ((text[1] & 0xC0U) == 0x80U)) {
        return 2U;
    }
    if ((remaining >= 3U) && ((text[2] & 0xC0U) == 0x80U)) {
        bool valid_second =
            ((text[0] == 0xE0U) && (text[1] >= 0xA0U) && (text[1] <= 0xBFU)) ||
            (((text[0] >= 0xE1U) && (text[0] <= 0xECU)) &&
             ((text[1] & 0xC0U) == 0x80U)) ||
            ((text[0] == 0xEDU) && (text[1] >= 0x80U) && (text[1] <= 0x9FU)) ||
            (((text[0] >= 0xEEU) && (text[0] <= 0xEFU)) &&
             ((text[1] & 0xC0U) == 0x80U));
        if (valid_second) {
            return 3U;
        }
    }
    if ((remaining >= 4U) &&
        ((text[2] & 0xC0U) == 0x80U) &&
        ((text[3] & 0xC0U) == 0x80U)) {
        bool valid_second =
            ((text[0] == 0xF0U) && (text[1] >= 0x90U) && (text[1] <= 0xBFU)) ||
            (((text[0] >= 0xF1U) && (text[0] <= 0xF3U)) &&
             ((text[1] & 0xC0U) == 0x80U)) ||
            ((text[0] == 0xF4U) && (text[1] >= 0x80U) && (text[1] <= 0x8FU));
        if (valid_second) {
            return 4U;
        }
    }
    return 0U;
}

static bool network_text_is_valid_utf8(const char *text, size_t length)
{
    size_t index = 0U;

    while (index < length) {
        size_t sequence_length = network_utf8_sequence_length(
            (const uint8_t *)text + index,
            length - index);
        if (sequence_length == 0U) {
            return false;
        }
        index += sequence_length;
    }
    return true;
}

static void network_copy_text(char *destination,
                              size_t destination_size,
                              const char *source)
{
    size_t length = strlen(source);

    memset(destination, 0, destination_size);
    memcpy(destination, source, length);
}

static bool network_credentials_are_valid(const char *ssid,
                                          const char *password)
{
    if ((ssid == NULL) || (password == NULL)) {
        return false;
    }

    size_t ssid_length = strnlen(ssid,
        WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U);
    size_t password_length = strnlen(password,
        WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U);
    if ((ssid_length == 0U) ||
        (ssid_length > WIFI_ALARM_PORTAL_SSID_MAX_LEN) ||
        (password_length > WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN) ||
        ((password_length > 0U) && (password_length < 8U))) {
        return false;
    }
    if (!network_text_is_valid_utf8(ssid, ssid_length)) {
        return false;
    }

    for (size_t index = 0U; index < ssid_length; ++index) {
        if ((uint8_t)ssid[index] < 0x20U) {
            return false;
        }
    }
    for (size_t index = 0U; index < password_length; ++index) {
        uint8_t character = (uint8_t)password[index];
        if ((character < 0x20U) || (character > 0x7EU)) {
            return false;
        }
    }
    return true;
}

static uint32_t network_record_checksum(const NetworkCredentialRecord *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    uint32_t hash = 2166136261UL;

    for (size_t index = 0U;
         index < offsetof(NetworkCredentialRecord, checksum);
         ++index) {
        hash ^= bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

static esp_err_t network_nvs_save(const char *ssid, const char *password)
{
    const bool configured = (ssid != NULL) && (ssid[0] != '\0');
    NetworkCredentialRecord record = {
        .magic = NETWORK_RECORD_MAGIC,
        .version = NETWORK_RECORD_VERSION,
        .configured = configured ? 1U : 0U,
    };
    nvs_handle_t handle;
    esp_err_t err;

    if ((ssid == NULL) || (password == NULL) ||
        (configured && !network_credentials_are_valid(ssid, password)) ||
        (!configured && (password[0] != '\0'))) {
        return ESP_ERR_INVALID_ARG;
    }
    network_copy_text(record.ssid, sizeof(record.ssid), ssid);
    network_copy_text(record.password, sizeof(record.password), password);
    record.checksum = network_record_checksum(&record);

    err = nvs_open(NETWORK_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        network_secure_zero(&record, sizeof(record));
        return err;
    }
    err = nvs_set_blob(handle, NETWORK_NVS_KEY, &record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    network_secure_zero(&record, sizeof(record));
    return err;
}

static esp_err_t network_nvs_load(char *ssid,
                                  size_t ssid_size,
                                  char *password,
                                  size_t password_size)
{
    NetworkCredentialRecord record = {0};
    size_t record_size = sizeof(record);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NETWORK_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_blob(handle, NETWORK_NVS_KEY, &record, &record_size);
    nvs_close(handle);
    if (err != ESP_OK) {
        network_secure_zero(&record, sizeof(record));
        return err;
    }

    bool valid = (record_size == sizeof(record)) &&
                 (record.magic == NETWORK_RECORD_MAGIC) &&
                 (record.version == NETWORK_RECORD_VERSION) &&
                 (record.configured <= 1U) &&
                 (record.checksum == network_record_checksum(&record)) &&
                 (memchr(record.ssid, '\0', sizeof(record.ssid)) != NULL) &&
                 (memchr(record.password, '\0', sizeof(record.password)) != NULL) &&
                 (((record.configured == 1U) &&
                   network_credentials_are_valid(record.ssid, record.password)) ||
                  ((record.configured == 0U) &&
                   (record.ssid[0] == '\0') &&
                   (record.password[0] == '\0'))) &&
                 (ssid_size >= sizeof(record.ssid)) &&
                 (password_size >= sizeof(record.password));
    if (valid) {
        memcpy(ssid, record.ssid, sizeof(record.ssid));
        memcpy(password, record.password, sizeof(record.password));
    }
    network_secure_zero(&record, sizeof(record));
    return valid ? ESP_OK : ESP_ERR_INVALID_CRC;
}

static esp_err_t network_set_state_credentials(const char *ssid,
                                               const char *password)
{
    esp_err_t err = alarm_lock();

    if (err != ESP_OK) {
        return err;
    }
    network_copy_text(s_portal.sta_ssid, sizeof(s_portal.sta_ssid), ssid);
    network_secure_zero(s_portal.sta_password, sizeof(s_portal.sta_password));
    network_copy_text(s_portal.sta_password,
                      sizeof(s_portal.sta_password),
                      password);
    alarm_unlock();
    return ESP_OK;
}

static esp_err_t network_activate_runtime(const char *ssid,
                                           const char *password)
{
    const bool enable_sta = (ssid[0] != '\0');
    wifi_config_t sta_config = {0};
    esp_err_t err;

    atomic_store(&s_portal.sta_suspended, true);
    const bool was_connected =
        (xEventGroupGetBits(s_portal.sta_event_group) &
         PORTAL_STA_CONNECTED_BIT) != 0U;
    xEventGroupClearBits(s_portal.sta_event_group,
                         PORTAL_STA_CONNECTED_BIT |
                         PORTAL_STA_FAILED_BIT |
                         PORTAL_STA_DISCONNECTED_BIT);

    /* Keep APSTA available even when no uplink is configured: active scans
     * require the STA interface. Deliberate disconnect completion is awaited
     * while retry handling remains suspended, so its reason=8 event cannot be
     * mistaken for an unexpected link loss. */
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        goto fail;
    }
    err = esp_wifi_disconnect();
    if ((err == ESP_OK) && was_connected) {
        EventBits_t bits = xEventGroupWaitBits(
            s_portal.sta_event_group,
            PORTAL_STA_DISCONNECTED_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(NETWORK_DISCONNECT_TIMEOUT_MS));
        if ((bits & PORTAL_STA_DISCONNECTED_BIT) == 0U) {
            err = ESP_ERR_TIMEOUT;
            goto fail;
        }
    } else if ((err != ESP_OK) && (err != ESP_ERR_WIFI_NOT_CONNECT)) {
        goto fail;
    }

    if (enable_sta) {
        size_t ssid_length = strlen(ssid);
        size_t password_length = strlen(password);
        memcpy(sta_config.sta.ssid, ssid, ssid_length);
        memcpy(sta_config.sta.password, password, password_length);
        sta_config.sta.threshold.authmode = (password_length == 0U) ?
            WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        sta_config.sta.pmf_cfg.capable = true;
        sta_config.sta.pmf_cfg.required = false;
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        if (err != ESP_OK) {
            goto fail;
        }
    }

    err = network_set_state_credentials(ssid, password);
    if (err != ESP_OK) {
        goto fail;
    }
    atomic_store(&s_portal.sta_enabled, enable_sta);
    atomic_store(&s_portal.sta_retry_count, 0U);
    atomic_store(&s_portal.sta_suspended, false);
    if (!enable_sta) {
        return ESP_OK;
    }
    return esp_wifi_connect();

fail:
    atomic_store(&s_portal.sta_suspended, false);
    return err;
}

static void network_provision_task(void *argument)
{
    NetworkProvisionContext *context = argument;
    esp_err_t err = network_activate_runtime(context->candidate_ssid,
                                             context->candidate_password);
    EventBits_t bits = 0U;

    if (err == ESP_OK) {
        bits = xEventGroupWaitBits(s_portal.sta_event_group,
                                   PORTAL_STA_CONNECTED_BIT |
                                   PORTAL_STA_FAILED_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   pdMS_TO_TICKS(NETWORK_PROVISION_TIMEOUT_MS));
    }

    if ((err == ESP_OK) && ((bits & PORTAL_STA_CONNECTED_BIT) != 0U)) {
        err = network_nvs_save(context->candidate_ssid,
                               context->candidate_password);
        if (err == ESP_OK) {
            atomic_store(&s_portal.provision_failure_reason, 0U);
            atomic_store(&s_portal.provision_result,
                         NETWORK_PROVISION_SUCCESS);
            ESP_LOGI(TAG, "station provisioning succeeded; credentials persisted");
        } else {
            ESP_LOGE(TAG, "station credential persistence failed: %s",
                     esp_err_to_name(err));
            (void)network_activate_runtime(context->previous_ssid,
                                           context->previous_password);
            atomic_store(&s_portal.provision_result,
                         NETWORK_PROVISION_STORAGE_FAILED);
        }
    } else {
        uint16_t failure_reason = atomic_load(&s_portal.sta_disconnect_reason);
        ESP_LOGW(TAG, "station provisioning failed: err=%s reason=%u",
                 esp_err_to_name(err == ESP_OK ? ESP_ERR_TIMEOUT : err),
                 (unsigned int)failure_reason);
        (void)network_activate_runtime(context->previous_ssid,
                                       context->previous_password);
        atomic_store(&s_portal.provision_failure_reason, failure_reason);
        atomic_store(&s_portal.provision_result, NETWORK_PROVISION_FAILED);
    }

    atomic_store(&s_portal.sta_provisioning, false);
    network_secure_zero(context, sizeof(*context));
    free(context);
    vTaskDelete(NULL);
}

static void alarm_set_default(WifiAlarmConfig *config)
{
    *config = (WifiAlarmConfig) {
        .schedule_id = 0U,
        .hour = ALARM_DEFAULT_HOUR,
        .minute = ALARM_DEFAULT_MINUTE,
        .weekday_mask = WIFI_ALARM_WEEKDAY_ALL,
        .repeat = true,
        .enabled = false,
    };
}

static esp_err_t alarm_nvs_save(const WifiAlarmConfig *config)
{
    AlarmNvsRecordV2 record = {
        .magic = ALARM_RECORD_MAGIC,
        .version = ALARM_RECORD_VERSION_V2,
        .schedule_id = config->schedule_id,
        .hour = config->hour,
        .minute = config->minute,
        .weekday_mask = config->weekday_mask,
        .repeat = config->repeat ? 1U : 0U,
        .enabled = config->enabled ? 1U : 0U,
    };
    nvs_handle_t handle;
    esp_err_t err;

    record.checksum = alarm_v2_checksum(&record);
    err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "open NVS for write failed");

    err = nvs_set_blob(handle, ALARM_NVS_KEY, &record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t alarm_nvs_load(WifiAlarmConfig *config)
{
    AlarmNvsRecord record = {0};
    size_t record_size = sizeof(record);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        alarm_set_default(config);
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open NVS for read failed");

    err = nvs_get_blob(handle, ALARM_NVS_KEY, record.bytes, &record_size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        alarm_set_default(config);
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "read alarm record failed");

    if (record_size != sizeof(AlarmNvsRecordV1)) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (record.v1.version == ALARM_RECORD_VERSION_V1) {
        if ((record.v1.magic != ALARM_RECORD_MAGIC) ||
            (record.v1.checksum != alarm_v1_checksum(&record.v1)) ||
            (record.v1.hour >= 24U) ||
            (record.v1.minute >= 60U) ||
            (record.v1.enabled > 1U)) {
            return ESP_ERR_INVALID_CRC;
        }

        *config = (WifiAlarmConfig) {
            .schedule_id = 0U,
            .hour = record.v1.hour,
            .minute = record.v1.minute,
            .weekday_mask = WIFI_ALARM_WEEKDAY_ALL,
            .repeat = true,
            .enabled = (record.v1.enabled != 0U),
        };
        err = alarm_nvs_save(config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "NVS alarm config migrated v1 -> v2");
        }
        return err;
    }

    if (record.v2.version != ALARM_RECORD_VERSION_V2) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if ((record.v2.magic != ALARM_RECORD_MAGIC) ||
        (record.v2.checksum != alarm_v2_checksum(&record.v2)) ||
        (record.v2.repeat > 1U) ||
        (record.v2.enabled > 1U)) {
        return ESP_ERR_INVALID_CRC;
    }

    *config = (WifiAlarmConfig) {
        .schedule_id = record.v2.schedule_id,
        .hour = record.v2.hour,
        .minute = record.v2.minute,
        .weekday_mask = record.v2.weekday_mask,
        .repeat = (record.v2.repeat != 0U),
        .enabled = (record.v2.enabled != 0U),
    };
    return alarm_config_is_valid(config) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t portal_publish_config(const WifiAlarmConfig *config)
{
    esp_err_t err = ESP_OK;

    if (s_portal.options.apply_callback != NULL) {
        err = s_portal.options.apply_callback(config,
                                              s_portal.options.user_context);
    }
    if ((err == ESP_OK) && (s_portal.options.event_callback != NULL)) {
        const WifiAlarmPortalEvent event = {
            .type = WIFI_ALARM_PORTAL_EVENT_CONFIG_UPDATED,
            .data.config = *config,
        };
        err = s_portal.options.event_callback(&event,
                                              s_portal.options.user_context);
    }
    return err;
}

static esp_err_t portal_send_json(httpd_req_t *request,
                                  const char *status,
                                  const char *json)
{
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
}

static esp_err_t portal_send_error(httpd_req_t *request,
                                   const char *status,
                                   const char *code)
{
    char response[96];
    int length = snprintf(response, sizeof(response),
                          "{\"ok\":false,\"error\":\"%s\"}", code);
    if ((length < 0) || ((size_t)length >= sizeof(response))) {
        return ESP_FAIL;
    }
    return portal_send_json(request, status, response);
}

static esp_err_t portal_receive_body(httpd_req_t *request,
                                     char *body,
                                     size_t body_size)
{
    size_t received = 0U;
    unsigned int timeout_count = 0U;

    if ((request->content_len <= 0) ||
        ((size_t)request->content_len >= body_size) ||
        ((size_t)request->content_len > ALARM_HTTP_BODY_MAX_LEN)) {
        return ESP_ERR_INVALID_SIZE;
    }

    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(request, body + received,
                                    (size_t)request->content_len - received);
        if (result == HTTPD_SOCK_ERR_TIMEOUT) {
            ++timeout_count;
            if (timeout_count >= ALARM_HTTP_RECV_MAX_TIMEOUTS) {
                return ESP_ERR_TIMEOUT;
            }
            continue;
        }
        if (result <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)result;
    }
    body[received] = '\0';
    return ESP_OK;
}

typedef struct {
    const char *cursor;
    const char *end;
} PortalJsonCursor;

static void portal_json_skip_whitespace(PortalJsonCursor *json)
{
    while ((json->cursor < json->end) &&
           ((*json->cursor == ' ') || (*json->cursor == '\t') ||
            (*json->cursor == '\r') || (*json->cursor == '\n'))) {
        ++json->cursor;
    }
}

static bool portal_json_consume(PortalJsonCursor *json, char expected)
{
    portal_json_skip_whitespace(json);
    if ((json->cursor >= json->end) || (*json->cursor != expected)) {
        return false;
    }
    ++json->cursor;
    return true;
}

static bool portal_json_parse_name(PortalJsonCursor *json,
                                   char *name,
                                   size_t name_size)
{
    size_t length = 0U;

    if (!portal_json_consume(json, '"')) {
        return false;
    }
    while ((json->cursor < json->end) && (*json->cursor != '"')) {
        unsigned char character = (unsigned char)*json->cursor;
        if ((character < 0x20U) || (character > 0x7EU) ||
            (character == '\\') || ((length + 1U) >= name_size)) {
            return false;
        }
        name[length++] = *json->cursor++;
    }
    if ((json->cursor >= json->end) || (*json->cursor != '"')) {
        return false;
    }
    ++json->cursor;
    name[length] = '\0';
    return portal_json_consume(json, ':');
}

static int portal_json_hex_value(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return character - '0';
    }
    if ((character >= 'a') && (character <= 'f')) {
        return character - 'a' + 10;
    }
    if ((character >= 'A') && (character <= 'F')) {
        return character - 'A' + 10;
    }
    return -1;
}

static bool portal_json_append_utf8(uint32_t code_point,
                                    char *output,
                                    size_t output_size,
                                    size_t *output_length)
{
    uint8_t encoded[3];
    size_t encoded_length;

    if ((code_point == 0U) || (code_point < 0x20U) ||
        ((code_point >= 0xD800U) && (code_point <= 0xDFFFU))) {
        return false;
    }
    if (code_point <= 0x7FU) {
        encoded[0] = (uint8_t)code_point;
        encoded_length = 1U;
    } else if (code_point <= 0x7FFU) {
        encoded[0] = (uint8_t)(0xC0U | (code_point >> 6U));
        encoded[1] = (uint8_t)(0x80U | (code_point & 0x3FU));
        encoded_length = 2U;
    } else {
        encoded[0] = (uint8_t)(0xE0U | (code_point >> 12U));
        encoded[1] = (uint8_t)(0x80U | ((code_point >> 6U) & 0x3FU));
        encoded[2] = (uint8_t)(0x80U | (code_point & 0x3FU));
        encoded_length = 3U;
    }
    if ((*output_length + encoded_length) >= output_size) {
        return false;
    }
    memcpy(output + *output_length, encoded, encoded_length);
    *output_length += encoded_length;
    return true;
}

static bool portal_json_parse_string(PortalJsonCursor *json,
                                     char *output,
                                     size_t output_size)
{
    size_t output_length = 0U;

    if ((output == NULL) || (output_size == 0U) ||
        !portal_json_consume(json, '"')) {
        return false;
    }
    while (json->cursor < json->end) {
        uint8_t character = (uint8_t)*json->cursor++;

        if (character == '"') {
            output[output_length] = '\0';
            return true;
        }
        if (character < 0x20U) {
            return false;
        }
        if (character != '\\') {
            if ((output_length + 1U) >= output_size) {
                return false;
            }
            output[output_length++] = (char)character;
            continue;
        }
        if (json->cursor >= json->end) {
            return false;
        }

        character = (uint8_t)*json->cursor++;
        if ((character == '"') || (character == '\\') ||
            (character == '/')) {
            if ((output_length + 1U) >= output_size) {
                return false;
            }
            output[output_length++] = (char)character;
            continue;
        }
        if (character != 'u' || ((json->end - json->cursor) < 4)) {
            return false;
        }

        uint32_t code_point = 0U;
        for (size_t index = 0U; index < 4U; ++index) {
            int value = portal_json_hex_value(json->cursor[index]);
            if (value < 0) {
                return false;
            }
            code_point = (code_point << 4U) | (uint32_t)value;
        }
        json->cursor += 4;
        if (!portal_json_append_utf8(code_point,
                                     output,
                                     output_size,
                                     &output_length)) {
            return false;
        }
    }
    return false;
}

static bool portal_json_parse_int64(PortalJsonCursor *json, int64_t *value)
{
    bool negative = false;
    uint64_t magnitude = 0U;
    uint64_t limit;
    const char *first_digit;

    portal_json_skip_whitespace(json);
    if ((json->cursor < json->end) && (*json->cursor == '-')) {
        negative = true;
        ++json->cursor;
    }
    first_digit = json->cursor;
    if ((json->cursor >= json->end) ||
        (*json->cursor < '0') || (*json->cursor > '9')) {
        return false;
    }
    if ((*json->cursor == '0') && ((json->cursor + 1) < json->end) &&
        (json->cursor[1] >= '0') && (json->cursor[1] <= '9')) {
        return false;
    }

    limit = negative ? ((uint64_t)INT64_MAX + 1U) : (uint64_t)INT64_MAX;
    while ((json->cursor < json->end) &&
           (*json->cursor >= '0') && (*json->cursor <= '9')) {
        uint8_t digit = (uint8_t)(*json->cursor - '0');
        if (magnitude > ((limit - digit) / 10U)) {
            return false;
        }
        magnitude = (magnitude * 10U) + digit;
        ++json->cursor;
    }
    if (json->cursor == first_digit) {
        return false;
    }

    if (negative) {
        *value = (magnitude == ((uint64_t)INT64_MAX + 1U)) ?
            INT64_MIN : -(int64_t)magnitude;
    } else {
        *value = (int64_t)magnitude;
    }
    return true;
}

static bool portal_json_parse_bool(PortalJsonCursor *json, bool *value)
{
    portal_json_skip_whitespace(json);
    if (((size_t)(json->end - json->cursor) >= 4U) &&
        (memcmp(json->cursor, "true", 4U) == 0)) {
        json->cursor += 4;
        *value = true;
        return true;
    }
    if (((size_t)(json->end - json->cursor) >= 5U) &&
        (memcmp(json->cursor, "false", 5U) == 0)) {
        json->cursor += 5;
        *value = false;
        return true;
    }
    return false;
}

static bool portal_json_next_field(PortalJsonCursor *json,
                                   bool first_field,
                                   bool *object_done)
{
    portal_json_skip_whitespace(json);
    if ((json->cursor < json->end) && (*json->cursor == '}')) {
        ++json->cursor;
        *object_done = true;
        return true;
    }
    *object_done = false;
    return first_field || portal_json_consume(json, ',');
}

static bool portal_json_is_finished(PortalJsonCursor *json)
{
    portal_json_skip_whitespace(json);
    return json->cursor == json->end;
}

static bool portal_parse_alarm_json(const char *body,
                                    size_t length,
                                    WifiAlarmConfig *config)
{
    enum {
        FIELD_SCHEDULE_ID = (1U << 0),
        FIELD_HOUR = (1U << 1),
        FIELD_MINUTE = (1U << 2),
        FIELD_WEEKDAY_MASK = (1U << 3),
        FIELD_REPEAT = (1U << 4),
        FIELD_ENABLED = (1U << 5),
        FIELD_ALL = 0x3FU,
    };
    PortalJsonCursor json = {.cursor = body, .end = body + length};
    uint32_t fields_seen = 0U;
    bool first_field = true;
    bool object_done = false;

    if ((config == NULL) || !portal_json_consume(&json, '{')) {
        return false;
    }
    while (!object_done) {
        char name[24];
        uint32_t field_bit;
        int64_t integer_value;

        if (!portal_json_next_field(&json, first_field, &object_done)) {
            return false;
        }
        first_field = false;
        if (object_done) {
            break;
        }
        if (!portal_json_parse_name(&json, name, sizeof(name))) {
            return false;
        }

        if (strcmp(name, "schedule_id") == 0) {
            field_bit = FIELD_SCHEDULE_ID;
            if (!portal_json_parse_int64(&json, &integer_value) ||
                (integer_value != 0)) {
                return false;
            }
            config->schedule_id = 0U;
        } else if (strcmp(name, "hour") == 0) {
            field_bit = FIELD_HOUR;
            if (!portal_json_parse_int64(&json, &integer_value) ||
                (integer_value < 0) || (integer_value > 23)) {
                return false;
            }
            config->hour = (uint8_t)integer_value;
        } else if (strcmp(name, "minute") == 0) {
            field_bit = FIELD_MINUTE;
            if (!portal_json_parse_int64(&json, &integer_value) ||
                (integer_value < 0) || (integer_value > 59)) {
                return false;
            }
            config->minute = (uint8_t)integer_value;
        } else if (strcmp(name, "weekday_mask") == 0) {
            field_bit = FIELD_WEEKDAY_MASK;
            if (!portal_json_parse_int64(&json, &integer_value) ||
                (integer_value < 0) ||
                (integer_value > WIFI_ALARM_WEEKDAY_ALL)) {
                return false;
            }
            config->weekday_mask = (uint8_t)integer_value;
        } else if (strcmp(name, "repeat") == 0) {
            field_bit = FIELD_REPEAT;
            if (!portal_json_parse_bool(&json, &config->repeat)) {
                return false;
            }
        } else if (strcmp(name, "enabled") == 0) {
            field_bit = FIELD_ENABLED;
            if (!portal_json_parse_bool(&json, &config->enabled)) {
                return false;
            }
        } else {
            return false;
        }

        if ((fields_seen & field_bit) != 0U) {
            return false;
        }
        fields_seen |= field_bit;
    }

    return (fields_seen == FIELD_ALL) && portal_json_is_finished(&json) &&
           alarm_config_is_valid(config);
}

static bool portal_parse_time_json(const char *body,
                                   size_t length,
                                   int64_t *epoch_seconds,
                                   int16_t *utc_offset_minutes)
{
    enum {
        FIELD_EPOCH = (1U << 0),
        FIELD_OFFSET = (1U << 1),
        FIELD_ALL = 0x03U,
    };
    PortalJsonCursor json = {.cursor = body, .end = body + length};
    uint32_t fields_seen = 0U;
    bool first_field = true;
    bool object_done = false;

    if ((epoch_seconds == NULL) || (utc_offset_minutes == NULL) ||
        !portal_json_consume(&json, '{')) {
        return false;
    }
    while (!object_done) {
        char name[24];
        uint32_t field_bit;
        int64_t integer_value;

        if (!portal_json_next_field(&json, first_field, &object_done)) {
            return false;
        }
        first_field = false;
        if (object_done) {
            break;
        }
        if (!portal_json_parse_name(&json, name, sizeof(name)) ||
            !portal_json_parse_int64(&json, &integer_value)) {
            return false;
        }

        if (strcmp(name, "epoch_seconds") == 0) {
            field_bit = FIELD_EPOCH;
            if ((integer_value < ALARM_MIN_EPOCH_SECONDS) ||
                (integer_value > ALARM_MAX_EPOCH_SECONDS)) {
                return false;
            }
            *epoch_seconds = integer_value;
        } else if (strcmp(name, "utc_offset_minutes") == 0) {
            field_bit = FIELD_OFFSET;
            if ((integer_value < ALARM_MIN_UTC_OFFSET_MIN) ||
                (integer_value > ALARM_MAX_UTC_OFFSET_MIN)) {
                return false;
            }
            *utc_offset_minutes = (int16_t)integer_value;
        } else {
            return false;
        }

        if ((fields_seen & field_bit) != 0U) {
            return false;
        }
        fields_seen |= field_bit;
    }

    return (fields_seen == FIELD_ALL) && portal_json_is_finished(&json);
}

static bool portal_parse_network_json(const char *body,
                                      size_t length,
                                      char *ssid,
                                      size_t ssid_size,
                                      char *password,
                                      size_t password_size)
{
    enum {
        FIELD_SSID = (1U << 0),
        FIELD_PASSWORD = (1U << 1),
        FIELD_ALL = FIELD_SSID | FIELD_PASSWORD,
    };
    PortalJsonCursor json = {.cursor = body, .end = body + length};
    uint32_t fields_seen = 0U;
    bool first_field = true;
    bool object_done = false;

    if ((body == NULL) || (ssid == NULL) || (password == NULL) ||
        !portal_json_consume(&json, '{')) {
        return false;
    }

    while (portal_json_next_field(&json, first_field, &object_done)) {
        char name[16] = {0};
        uint32_t field_bit;

        if (object_done) {
            break;
        }
        first_field = false;
        if (!portal_json_parse_name(&json, name, sizeof(name))) {
            return false;
        }
        if (strcmp(name, "ssid") == 0) {
            field_bit = FIELD_SSID;
            if (!portal_json_parse_string(&json, ssid, ssid_size)) {
                return false;
            }
        } else if (strcmp(name, "password") == 0) {
            field_bit = FIELD_PASSWORD;
            if (!portal_json_parse_string(&json, password, password_size)) {
                return false;
            }
        } else {
            return false;
        }
        if ((fields_seen & field_bit) != 0U) {
            return false;
        }
        fields_seen |= field_bit;
    }

    return (fields_seen == FIELD_ALL) &&
           object_done &&
           portal_json_is_finished(&json) &&
           network_credentials_are_valid(ssid, password);
}

static const char *network_provision_result_text(int result)
{
    switch ((NetworkProvisionResult)result) {
    case NETWORK_PROVISION_IDLE:
        return "idle";
    case NETWORK_PROVISION_CONNECTING:
        return "connecting";
    case NETWORK_PROVISION_SUCCESS:
        return "success";
    case NETWORK_PROVISION_FAILED:
        return "failed";
    case NETWORK_PROVISION_CLEARED:
        return "cleared";
    case NETWORK_PROVISION_STORAGE_FAILED:
        return "storage_failed";
    default:
        return "unknown";
    }
}

static bool portal_json_escape_text(const char *input,
                                    char *output,
                                    size_t output_size)
{
    const size_t input_length = (input != NULL) ? strlen(input) : 0U;
    size_t output_length = 0U;

    if ((input == NULL) || (output == NULL) || (output_size == 0U)) {
        return false;
    }
    for (size_t index = 0U; index < input_length; ++index) {
        const uint8_t character = (uint8_t)input[index];
        const char *escape = NULL;

        if (character >= 0x80U) {
            size_t sequence_length = network_utf8_sequence_length(
                (const uint8_t *)input + index,
                input_length - index);
            if (sequence_length == 0U) {
                static const char replacement[] = "\\uFFFD";
                if ((output_length + sizeof(replacement) - 1U) >= output_size) {
                    return false;
                }
                memcpy(output + output_length,
                       replacement,
                       sizeof(replacement) - 1U);
                output_length += sizeof(replacement) - 1U;
                continue;
            }
            if ((output_length + sequence_length) >= output_size) {
                return false;
            }
            memcpy(output + output_length, input + index, sequence_length);
            output_length += sequence_length;
            index += sequence_length - 1U;
            continue;
        }

        if (character == '"') {
            escape = "\\\"";
        } else if (character == '\\') {
            escape = "\\\\";
        }
        if (escape != NULL) {
            if ((output_length + 2U) >= output_size) {
                return false;
            }
            memcpy(output + output_length, escape, 2U);
            output_length += 2U;
        } else if (character < 0x20U) {
            if ((output_length + 6U) >= output_size) {
                return false;
            }
            int written = snprintf(output + output_length,
                                   output_size - output_length,
                                   "\\u%04X",
                                   (unsigned int)character);
            if (written != 6) {
                return false;
            }
            output_length += 6U;
        } else {
            if ((output_length + 1U) >= output_size) {
                return false;
            }
            output[output_length++] = (char)character;
        }
    }
    output[output_length] = '\0';
    return true;
}

static esp_err_t network_status_get_handler(httpd_req_t *request)
{
    char ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U] = {0};
    char escaped_ssid[(WIFI_ALARM_PORTAL_SSID_MAX_LEN * 6U) + 1U] = {0};
    char response[384];
    EventBits_t bits = xEventGroupGetBits(s_portal.sta_event_group);
    esp_err_t err = alarm_lock();

    if (err != ESP_OK) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "network_state_unavailable");
    }
    memcpy(ssid, s_portal.sta_ssid, sizeof(ssid));
    alarm_unlock();

    if (!portal_json_escape_text(ssid, escaped_ssid, sizeof(escaped_ssid))) {
        return portal_send_error(request, HTTPD_500, "response_encode_failed");
    }
    int length = snprintf(
        response,
        sizeof(response),
        "{\"ok\":true,\"configured\":%s,\"ssid\":\"%s\","
        "\"connected\":%s,\"provisioning\":%s,\"result\":\"%s\","
        "\"disconnect_reason\":%u}",
        ssid[0] != '\0' ? "true" : "false",
        escaped_ssid,
        (bits & PORTAL_STA_CONNECTED_BIT) != 0U ? "true" : "false",
        atomic_load(&s_portal.sta_provisioning) ? "true" : "false",
        network_provision_result_text(atomic_load(&s_portal.provision_result)),
        (unsigned int)atomic_load(&s_portal.provision_failure_reason));
    if ((length < 0) || ((size_t)length >= sizeof(response))) {
        return portal_send_error(request, HTTPD_500, "response_encode_failed");
    }
    return portal_send_json(request, HTTPD_200, response);
}

static esp_err_t network_config_put_handler(httpd_req_t *request)
{
    char body[ALARM_HTTP_BODY_MAX_LEN + 1U] = {0};
    char ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U] = {0};
    char password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U] = {0};
    bool expected = false;
    esp_err_t err = portal_receive_body(request, body, sizeof(body));

    if ((err != ESP_OK) ||
        !portal_parse_network_json(body,
                                   (size_t)request->content_len,
                                   ssid,
                                   sizeof(ssid),
                                   password,
                                   sizeof(password))) {
        network_secure_zero(password, sizeof(password));
        return portal_send_error(request, HTTPD_400, "invalid_network_config");
    }
    if (atomic_load(&s_portal.sta_suspended) ||
        atomic_load(&s_portal.scan_running) ||
        !atomic_compare_exchange_strong(&s_portal.sta_provisioning,
                                        &expected,
                                        true)) {
        network_secure_zero(password, sizeof(password));
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_CONFLICT,
                                 "network_busy");
    }

    NetworkProvisionContext *context = calloc(1U, sizeof(*context));
    if (context == NULL) {
        atomic_store(&s_portal.sta_provisioning, false);
        network_secure_zero(password, sizeof(password));
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "no_memory");
    }
    network_copy_text(context->candidate_ssid,
                      sizeof(context->candidate_ssid), ssid);
    network_copy_text(context->candidate_password,
                      sizeof(context->candidate_password), password);
    network_secure_zero(password, sizeof(password));

    err = alarm_lock();
    if (err == ESP_OK) {
        memcpy(context->previous_ssid,
               s_portal.sta_ssid,
               sizeof(context->previous_ssid));
        memcpy(context->previous_password,
               s_portal.sta_password,
               sizeof(context->previous_password));
        alarm_unlock();
    }
    if (err != ESP_OK) {
        atomic_store(&s_portal.sta_provisioning, false);
        network_secure_zero(context, sizeof(*context));
        free(context);
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "network_state_unavailable");
    }

    atomic_store(&s_portal.provision_failure_reason, 0U);
    atomic_store(&s_portal.provision_result, NETWORK_PROVISION_CONNECTING);
    BaseType_t task_result = xTaskCreate(network_provision_task,
                                         "wifi_provision",
                                         NETWORK_PROVISION_TASK_STACK_SIZE,
                                         context,
                                         NETWORK_PROVISION_TASK_PRIORITY,
                                         NULL);
    if (task_result != pdPASS) {
        atomic_store(&s_portal.sta_provisioning, false);
        atomic_store(&s_portal.provision_result, NETWORK_PROVISION_FAILED);
        network_secure_zero(context, sizeof(*context));
        free(context);
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "task_create_failed");
    }
    return portal_send_json(
        request,
        PORTAL_HTTP_STATUS_ACCEPTED,
        "{\"ok\":true,\"message\":\"正在验证网络，成功后自动保存\"}");
}

static esp_err_t network_config_delete_handler(httpd_req_t *request)
{
    char previous_ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U] = {0};
    char previous_password[WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U] = {0};
    esp_err_t err;

    if (request->content_len != 0) {
        return portal_send_error(request, HTTPD_400, "unexpected_body");
    }
    if (atomic_load(&s_portal.sta_suspended) ||
        atomic_load(&s_portal.sta_provisioning) ||
        atomic_load(&s_portal.scan_running)) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_CONFLICT,
                                 "network_busy");
    }

    err = alarm_lock();
    if (err == ESP_OK) {
        memcpy(previous_ssid, s_portal.sta_ssid, sizeof(previous_ssid));
        memcpy(previous_password,
               s_portal.sta_password,
               sizeof(previous_password));
        alarm_unlock();
    }
    if (err == ESP_OK) {
        err = network_activate_runtime("", "");
    }
    if (err == ESP_OK) {
        err = network_nvs_save("", "");
    }
    if (err != ESP_OK) {
        (void)network_activate_runtime(previous_ssid, previous_password);
        network_secure_zero(previous_password, sizeof(previous_password));
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "network_clear_failed");
    }

    network_secure_zero(previous_password, sizeof(previous_password));
    atomic_store(&s_portal.provision_failure_reason, 0U);
    atomic_store(&s_portal.provision_result, NETWORK_PROVISION_CLEARED);
    return portal_send_json(request,
                            HTTPD_200,
                            "{\"ok\":true,\"message\":\"网络配置已清除\"}");
}

static esp_err_t network_scan_post_handler(httpd_req_t *request)
{
    bool expected = false;
    wifi_scan_config_t scan_config = {0};

    if (request->content_len != 0) {
        return portal_send_error(request, HTTPD_400, "unexpected_body");
    }
    if (atomic_load(&s_portal.sta_suspended) ||
        atomic_load(&s_portal.sta_provisioning) ||
        !atomic_compare_exchange_strong(&s_portal.scan_running,
                                        &expected,
                                        true)) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_CONFLICT,
                                 "network_busy");
    }

    atomic_store(&s_portal.scan_ready, false);
    xEventGroupClearBits(s_portal.sta_event_group, PORTAL_SCAN_DONE_BIT);
    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        atomic_store(&s_portal.scan_running, false);
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "scan_start_failed");
    }
    return portal_send_json(request,
                            PORTAL_HTTP_STATUS_ACCEPTED,
                            "{\"ok\":true,\"message\":\"扫描已开始\"}");
}

static esp_err_t network_scan_get_handler(httpd_req_t *request)
{
    char *response = calloc(1U, NETWORK_SCAN_RESPONSE_MAX_LEN);
    size_t response_length = 0U;
    esp_err_t err = ESP_OK;

    if (response == NULL) {
        return portal_send_error(request, HTTPD_500, "response_no_memory");
    }
    int written = snprintf(response,
                           NETWORK_SCAN_RESPONSE_MAX_LEN,
                           "{\"ok\":true,\"scanning\":%s,\"networks\":[",
                           atomic_load(&s_portal.scan_running) ? "true" : "false");
    if ((written < 0) || ((size_t)written >= NETWORK_SCAN_RESPONSE_MAX_LEN)) {
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    response_length = (size_t)written;

    if (atomic_load(&s_portal.scan_ready)) {
        uint16_t count = 0U;
        err = esp_wifi_scan_get_ap_num(&count);
        if (err != ESP_OK) {
            goto cleanup;
        }
        if (count > NETWORK_SCAN_MAX_RESULTS) {
            count = NETWORK_SCAN_MAX_RESULTS;
        }
        wifi_ap_record_t records[NETWORK_SCAN_MAX_RESULTS] = {0};
        err = esp_wifi_scan_get_ap_records(&count, records);
        if (err != ESP_OK) {
            goto cleanup;
        }
        atomic_store(&s_portal.scan_ready, false);

        for (uint16_t index = 0U; index < count; ++index) {
            char escaped_ssid[(WIFI_ALARM_PORTAL_SSID_MAX_LEN * 6U) + 1U] = {0};

            records[index].ssid[WIFI_ALARM_PORTAL_SSID_MAX_LEN] = '\0';
            if (!portal_json_escape_text((const char *)records[index].ssid,
                                         escaped_ssid,
                                         sizeof(escaped_ssid))) {
                err = ESP_ERR_INVALID_RESPONSE;
                goto cleanup;
            }
            written = snprintf(
                response + response_length,
                NETWORK_SCAN_RESPONSE_MAX_LEN - response_length,
                "%s{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
                index == 0U ? "" : ",",
                escaped_ssid,
                (int)records[index].rssi,
                (int)records[index].authmode);
            if ((written < 0) ||
                ((size_t)written >=
                 (NETWORK_SCAN_RESPONSE_MAX_LEN - response_length))) {
                err = ESP_ERR_INVALID_SIZE;
                goto cleanup;
            }
            response_length += (size_t)written;
        }
    }

    written = snprintf(response + response_length,
                       NETWORK_SCAN_RESPONSE_MAX_LEN - response_length,
                       "]}");
    if ((written < 0) ||
        ((size_t)written >= (NETWORK_SCAN_RESPONSE_MAX_LEN - response_length))) {
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    err = portal_send_json(request, HTTPD_200, response);

cleanup:
    free(response);
    if (err == ESP_OK) {
        return ESP_OK;
    }
    return portal_send_error(request,
                             PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                             "scan_result_failed");
}

static esp_err_t root_get_handler(httpd_req_t *request)
{
    size_t html_size = (size_t)(portal_index_html_end -
                                portal_index_html_start);
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request,
                           (const char *)portal_index_html_start,
                           html_size);
}

static esp_err_t status_get_handler(httpd_req_t *request)
{
    char response[ALARM_HTTP_RESPONSE_LEN];
    WifiAlarmConfig config;
    WifiAlarmRuntimeStatus runtime = {0};
    esp_err_t err = wifi_alarm_portal_get_config(&config);

    if (err != ESP_OK) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "config_unavailable");
    }
    if (s_portal.options.status_callback != NULL) {
        err = s_portal.options.status_callback(&runtime,
                                               s_portal.options.user_context);
        if (err != ESP_OK) {
            return portal_send_error(request,
                                     PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                     "runtime_unavailable");
        }
    }

    int length = snprintf(
        response,
        sizeof(response),
        "{\"ok\":true,\"epoch_seconds\":%" PRId64
        ",\"utc_offset_minutes\":%d,\"clock_synced\":%s,\"ringing\":%s,"
        "\"alarm\":{\"schedule_id\":%u,\"hour\":%u,\"minute\":%u,"
        "\"weekday_mask\":%u,\"repeat\":%s,\"enabled\":%s}}",
        runtime.epoch_seconds,
        runtime.utc_offset_minutes,
        runtime.clock_synced ? "true" : "false",
        runtime.ringing ? "true" : "false",
        config.schedule_id,
        config.hour,
        config.minute,
        config.weekday_mask,
        config.repeat ? "true" : "false",
        config.enabled ? "true" : "false");
    if ((length < 0) || ((size_t)length >= sizeof(response))) {
        return portal_send_error(request,
                                 HTTPD_500,
                                 "response_too_large");
    }
    return portal_send_json(request, HTTPD_200, response);
}

static esp_err_t alarm_put_handler(httpd_req_t *request)
{
    char body[ALARM_HTTP_BODY_MAX_LEN + 1U] = {0};
    WifiAlarmConfig config = {0};
    esp_err_t err = portal_receive_body(request, body, sizeof(body));

    if (err != ESP_OK) {
        return portal_send_error(request,
                                 HTTPD_400,
                                 "invalid_body");
    }

    if (!portal_parse_alarm_json(body,
                                 (size_t)request->content_len,
                                 &config)) {
        return portal_send_error(request,
                                 HTTPD_400,
                                 "invalid_alarm");
    }

    err = wifi_alarm_portal_set_config(&config);
    if (err != ESP_OK) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "config_update_failed");
    }
    return portal_send_json(
        request,
        HTTPD_200,
        "{\"ok\":true,\"message\":\"闹钟已保存\"}");
}

static esp_err_t time_sync_handler(httpd_req_t *request)
{
    char body[ALARM_HTTP_BODY_MAX_LEN + 1U] = {0};
    WifiAlarmPortalEvent event = {
        .type = WIFI_ALARM_PORTAL_EVENT_TIME_SYNC,
    };
    esp_err_t err = portal_receive_body(request, body, sizeof(body));

    if (err != ESP_OK) {
        return portal_send_error(request,
                                 HTTPD_400,
                                 "invalid_body");
    }

    if (!portal_parse_time_json(body,
                                (size_t)request->content_len,
                                &event.data.time_sync.epoch_seconds,
                                &event.data.time_sync.utc_offset_minutes)) {
        return portal_send_error(request,
                                 HTTPD_400,
                                 "invalid_time");
    }

    if (s_portal.options.event_callback == NULL) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "controller_unavailable");
    }
    err = s_portal.options.event_callback(&event,
                                          s_portal.options.user_context);
    if (err != ESP_OK) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "controller_busy");
    }
    return portal_send_json(
        request,
        HTTPD_200,
        "{\"ok\":true,\"message\":\"时间同步成功\"}");
}

static esp_err_t alarm_stop_handler(httpd_req_t *request)
{
    const WifiAlarmPortalEvent event = {
        .type = WIFI_ALARM_PORTAL_EVENT_STOP_CURRENT_RINGING,
    };
    esp_err_t err;

    if (request->content_len != 0) {
        return portal_send_error(request,
                                 HTTPD_400,
                                 "unexpected_body");
    }
    if (s_portal.options.event_callback == NULL) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "controller_unavailable");
    }
    err = s_portal.options.event_callback(&event,
                                          s_portal.options.user_context);
    if (err != ESP_OK) {
        return portal_send_error(request,
                                 PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE,
                                 "controller_busy");
    }
    return portal_send_json(
        request,
        HTTPD_200,
        "{\"ok\":true,\"message\":\"本次响铃已停止\"}");
}

static esp_err_t redirect_get_handler(httpd_req_t *request)
{
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", "/");
    return httpd_resp_send(request, NULL, 0U);
}

static esp_err_t redirect_not_found_handler(httpd_req_t *request,
                                            httpd_err_code_t error)
{
    (void)error;
    return redirect_get_handler(request);
}

static esp_err_t alarm_http_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16U;
    const httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get_handler},
        {.uri = "/api/v1/status", .method = HTTP_GET,
         .handler = status_get_handler},
        {.uri = "/api/v1/alarm/0", .method = HTTP_PUT,
         .handler = alarm_put_handler},
        {.uri = "/api/v1/time/sync", .method = HTTP_POST,
         .handler = time_sync_handler},
        {.uri = "/api/v1/alarm/stop", .method = HTTP_POST,
         .handler = alarm_stop_handler},
        {.uri = "/api/v1/network/status", .method = HTTP_GET,
         .handler = network_status_get_handler},
        {.uri = "/api/v1/network/config", .method = HTTP_PUT,
         .handler = network_config_put_handler},
        {.uri = "/api/v1/network/config", .method = HTTP_DELETE,
         .handler = network_config_delete_handler},
        {.uri = "/api/v1/network/scan", .method = HTTP_POST,
         .handler = network_scan_post_handler},
        {.uri = "/api/v1/network/scan", .method = HTTP_GET,
         .handler = network_scan_get_handler},
        {.uri = "/generate_204", .method = HTTP_GET,
         .handler = redirect_get_handler},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET,
         .handler = redirect_get_handler},
        {.uri = "/connecttest.txt", .method = HTTP_GET,
         .handler = redirect_get_handler},
        {.uri = "/ncsi.txt", .method = HTTP_GET,
         .handler = redirect_get_handler},
    };

    esp_err_t err = httpd_start(&s_portal.http_server, &config);
    ESP_RETURN_ON_ERROR(err, TAG, "HTTP server start failed");

    for (size_t index = 0U;
         index < (sizeof(handlers) / sizeof(handlers[0]));
         ++index) {
        err = httpd_register_uri_handler(s_portal.http_server,
                                         &handlers[index]);
        if (err != ESP_OK) {
            httpd_stop(s_portal.http_server);
            s_portal.http_server = NULL;
            return err;
        }
    }
    err = httpd_register_err_handler(s_portal.http_server,
                                     HTTPD_404_NOT_FOUND,
                                     redirect_not_found_handler);
    if (err != ESP_OK) {
        httpd_stop(s_portal.http_server);
        s_portal.http_server = NULL;
    }
    return err;
}

static esp_err_t validate_options(const WifiAlarmPortalOptions *options)
{
    if ((options == NULL) || (options->ap_ssid == NULL) ||
        (options->ap_password == NULL) ||
        (options->max_connections == 0U) ||
        (options->max_connections > ALARM_AP_MAX_CONNECTIONS)) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((options->apply_callback != NULL) &&
        (options->event_callback != NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool sta_enabled = (options->sta_ssid != NULL) &&
                             (options->sta_ssid[0] != '\0');
    if ((options->sta_maximum_retries == 0U) ||
        (sta_enabled && (options->sta_password == NULL))) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strnlen(options->ap_ssid,
                              WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U);
    size_t password_len = strnlen(options->ap_password,
                                  WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U);
    if ((ssid_len == 0U) ||
        (ssid_len > WIFI_ALARM_PORTAL_SSID_MAX_LEN) ||
        (password_len > WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN) ||
        ((password_len > 0U) && (password_len < 8U))) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sta_enabled) {
        ssid_len = strnlen(options->sta_ssid,
                           WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U);
        password_len = strnlen(options->sta_password,
                               WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U);
        if ((ssid_len == 0U) ||
            (ssid_len > WIFI_ALARM_PORTAL_SSID_MAX_LEN) ||
            (password_len > WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN) ||
            ((password_len > 0U) && (password_len < 8U))) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_start(const WifiAlarmPortalOptions *options)
{
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t ap_config = {0};
    wifi_config_t sta_config = {0};
    esp_err_t ret = validate_options(options);

    ESP_RETURN_ON_ERROR(ret, TAG, "invalid portal options");
    if (s_portal.running || (s_portal.mutex != NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    s_portal.mutex = xSemaphoreCreateMutex();
    if (s_portal.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_portal.options = *options;
    atomic_init(&s_portal.sta_enabled, false);
    atomic_init(&s_portal.sta_suspended, false);
    atomic_init(&s_portal.sta_provisioning, false);
    atomic_init(&s_portal.scan_running, false);
    atomic_init(&s_portal.scan_ready, false);
    atomic_init(&s_portal.sta_retry_count, 0U);
    atomic_init(&s_portal.sta_disconnect_reason, 0U);
    atomic_init(&s_portal.provision_failure_reason, 0U);
    atomic_init(&s_portal.provision_result, NETWORK_PROVISION_IDLE);
    s_portal.sta_event_group = xEventGroupCreate();
    if (s_portal.sta_event_group == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    ret = nvs_flash_init();
    ESP_GOTO_ON_ERROR(ret, fail, TAG,
                      "NVS init failed; storage was not erased");
    ESP_GOTO_ON_ERROR(alarm_nvs_load(&s_portal.alarm), fail, TAG,
                      "alarm load failed");
    ret = network_nvs_load(s_portal.sta_ssid,
                           sizeof(s_portal.sta_ssid),
                           s_portal.sta_password,
                           sizeof(s_portal.sta_password));
    if (ret == ESP_OK) {
        const bool persisted_sta_enabled = (s_portal.sta_ssid[0] != '\0');
        atomic_store(&s_portal.sta_enabled, persisted_sta_enabled);
        if (persisted_sta_enabled) {
            ESP_LOGI(TAG, "using persisted station credentials");
        } else {
            ESP_LOGI(TAG, "persisted station configuration is cleared");
        }
    } else {
        if ((ret != ESP_ERR_NVS_NOT_FOUND) &&
            (ret != ESP_ERR_NVS_NOT_INITIALIZED)) {
            ESP_LOGW(TAG,
                     "persisted station credentials unavailable: %s; "
                     "record retained",
                     esp_err_to_name(ret));
        }
        if ((options->sta_ssid != NULL) &&
            (options->sta_ssid[0] != '\0')) {
            network_copy_text(s_portal.sta_ssid,
                              sizeof(s_portal.sta_ssid),
                              options->sta_ssid);
            network_copy_text(s_portal.sta_password,
                              sizeof(s_portal.sta_password),
                              options->sta_password);
            atomic_store(&s_portal.sta_enabled, true);
            ESP_LOGI(TAG, "using build-time station credentials");
        }
    }

    ret = esp_netif_init();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        ESP_GOTO_ON_ERROR(ret, fail, TAG, "netif init failed");
    }
    ret = esp_event_loop_create_default();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        ESP_GOTO_ON_ERROR(ret, fail, TAG, "event loop init failed");
    }
    s_portal.ap_netif = esp_netif_create_default_wifi_ap();
    if (s_portal.ap_netif == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    s_portal.sta_netif = esp_netif_create_default_wifi_sta();
    if (s_portal.sta_netif == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    ESP_GOTO_ON_ERROR(esp_wifi_init(&wifi_init), fail, TAG,
                      "Wi-Fi init failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM),
                      fail_wifi, TAG, "Wi-Fi storage setup failed");
    ESP_GOTO_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            portal_wifi_event_handler,
                                            NULL,
                                            &s_portal.wifi_event_handler),
        fail_wifi, TAG, "Wi-Fi event handler registration failed");
    ESP_GOTO_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            portal_wifi_event_handler,
                                            NULL,
                                            &s_portal.ip_event_handler),
        fail_wifi, TAG, "IP event handler registration failed");

    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA),
                      fail_wifi, TAG, "Wi-Fi mode setup failed");

    size_t ssid_len = strlen(options->ap_ssid);
    size_t password_len = strlen(options->ap_password);
    memcpy(ap_config.ap.ssid, options->ap_ssid, ssid_len);
    ap_config.ap.ssid_len = (uint8_t)ssid_len;
    memcpy(ap_config.ap.password, options->ap_password, password_len);
    ap_config.ap.channel = 1U;
    ap_config.ap.max_connection = options->max_connections;
    ap_config.ap.authmode = (password_len == 0U) ?
        WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    ap_config.ap.pmf_cfg.required = false;

    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config),
                      fail_wifi, TAG, "Wi-Fi AP config failed");
    if (atomic_load(&s_portal.sta_enabled)) {
        ssid_len = strlen(s_portal.sta_ssid);
        password_len = strlen(s_portal.sta_password);
        memcpy(sta_config.sta.ssid, s_portal.sta_ssid, ssid_len);
        memcpy(sta_config.sta.password, s_portal.sta_password, password_len);
        sta_config.sta.threshold.authmode = (password_len == 0U) ?
            WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        sta_config.sta.pmf_cfg.capable = true;
        sta_config.sta.pmf_cfg.required = false;
        ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config),
                          fail_wifi, TAG, "Wi-Fi station config failed");
    }
    ESP_GOTO_ON_ERROR(esp_wifi_start(), fail_wifi, TAG,
                      "Wi-Fi start failed");
    s_portal.wifi_started = true;
    ESP_GOTO_ON_ERROR(alarm_http_start(), fail_wifi, TAG,
                      "portal HTTP start failed");
    esp_netif_ip_info_t ap_ip_info = {0};
    ESP_GOTO_ON_ERROR(esp_netif_get_ip_info(s_portal.ap_netif,
                                             &ap_ip_info),
                      fail_wifi, TAG, "SoftAP IP lookup failed");
    ESP_GOTO_ON_ERROR(portal_dns_server_start(ap_ip_info.ip.addr,
                                               &s_portal.dns_server),
                      fail_wifi, TAG, "captive DNS start failed");

    s_portal.running = true;
    ret = portal_publish_config(&s_portal.alarm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "initial config publish failed: %s",
                 esp_err_to_name(ret));
        (void)wifi_alarm_portal_stop();
        return ret;
    }

    ESP_LOGI(TAG, "SoftAP '%s' ready; open http://192.168.4.1",
             options->ap_ssid);
    if (atomic_load(&s_portal.sta_enabled)) {
        ESP_LOGI(TAG, "station uplink enabled; credentials are not logged");
    } else {
        ESP_LOGI(TAG, "station uplink not configured; use the local portal");
    }
    return ESP_OK;

fail_wifi:
    portal_dns_server_stop(s_portal.dns_server);
    s_portal.dns_server = NULL;
    if (s_portal.http_server != NULL) {
        (void)httpd_stop(s_portal.http_server);
        s_portal.http_server = NULL;
    }
    if (s_portal.wifi_started) {
        (void)esp_wifi_stop();
        s_portal.wifi_started = false;
    }
    if (s_portal.ip_event_handler != NULL) {
        (void)esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_portal.ip_event_handler);
    }
    if (s_portal.wifi_event_handler != NULL) {
        (void)esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_portal.wifi_event_handler);
    }
    (void)esp_wifi_deinit();
fail:
    if (s_portal.sta_netif != NULL) {
        esp_netif_destroy_default_wifi(s_portal.sta_netif);
    }
    if (s_portal.ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_portal.ap_netif);
    }
    if (s_portal.sta_event_group != NULL) {
        vEventGroupDelete(s_portal.sta_event_group);
    }
    vSemaphoreDelete(s_portal.mutex);
    memset(&s_portal, 0, sizeof(s_portal));
    return ret;
}

esp_err_t wifi_alarm_portal_wait_for_sta_ip(uint32_t timeout_ms)
{
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&s_portal.sta_enabled) ||
        atomic_load(&s_portal.sta_suspended) ||
        atomic_load(&s_portal.sta_provisioning) ||
        s_portal.sta_event_group == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_portal.sta_event_group,
        PORTAL_STA_CONNECTED_BIT | PORTAL_STA_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    if ((bits & PORTAL_STA_CONNECTED_BIT) != 0U) {
        return ESP_OK;
    }
    if ((bits & PORTAL_STA_FAILED_BIT) != 0U) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t wifi_alarm_portal_reconnect_sta(void)
{
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&s_portal.sta_enabled) ||
        atomic_load(&s_portal.sta_suspended) ||
        atomic_load(&s_portal.sta_provisioning) ||
        s_portal.sta_event_group == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    atomic_store(&s_portal.sta_retry_count, 0U);
    xEventGroupClearBits(s_portal.sta_event_group,
                         PORTAL_STA_CONNECTED_BIT | PORTAL_STA_FAILED_BIT);
    return esp_wifi_connect();
}

esp_err_t wifi_alarm_portal_suspend_sta(void)
{
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&s_portal.sta_enabled) ||
        s_portal.sta_event_group == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (atomic_load(&s_portal.sta_suspended)) {
        return ESP_OK;
    }

    atomic_store(&s_portal.sta_suspended, true);
    xEventGroupClearBits(s_portal.sta_event_group,
                         PORTAL_STA_CONNECTED_BIT | PORTAL_STA_FAILED_BIT);
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        atomic_store(&s_portal.sta_suspended, false);
        return err;
    }
    ESP_LOGI(TAG, "station suspended; SoftAP remains active");
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_resume_sta(void)
{
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&s_portal.sta_enabled) ||
        s_portal.sta_event_group == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!atomic_load(&s_portal.sta_suspended)) {
        return ESP_OK;
    }

    atomic_store(&s_portal.sta_retry_count, 0U);
    xEventGroupClearBits(s_portal.sta_event_group,
                         PORTAL_STA_CONNECTED_BIT | PORTAL_STA_FAILED_BIT);
    atomic_store(&s_portal.sta_suspended, false);
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        atomic_store(&s_portal.sta_suspended, true);
        return err;
    }
    ESP_LOGI(TAG, "station resumed; waiting for upstream connection");
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_get_config(WifiAlarmConfig *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(alarm_lock(), TAG, "alarm state lock failed");
    *config = s_portal.alarm;
    alarm_unlock();
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_set_config(const WifiAlarmConfig *config)
{
    WifiAlarmConfig previous_config;
    esp_err_t err;

    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!alarm_config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(alarm_lock(), TAG, "alarm state lock failed");
    previous_config = s_portal.alarm;
    alarm_unlock();

    err = alarm_nvs_save(config);
    ESP_RETURN_ON_ERROR(err, TAG, "alarm persistence failed");
    err = alarm_lock();
    if (err != ESP_OK) {
        esp_err_t rollback_err = alarm_nvs_save(&previous_config);
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "alarm rollback failed after lock timeout: %s",
                     esp_err_to_name(rollback_err));
        }
        return err;
    }
    s_portal.alarm = *config;
    alarm_unlock();

    err = portal_publish_config(config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "alarm publish failed after persistence: %s",
                 esp_err_to_name(err));
        esp_err_t rollback_err = alarm_nvs_save(&previous_config);
        if (rollback_err == ESP_OK) {
            rollback_err = alarm_lock();
            if (rollback_err == ESP_OK) {
                s_portal.alarm = previous_config;
                alarm_unlock();
            }
        }
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "alarm rollback failed: %s",
                     esp_err_to_name(rollback_err));
        }
        return err;
    }

    ESP_LOGI(TAG,
             "alarm updated: id=%u time=%02u:%02u weekdays=0x%02X "
             "repeat=%u enabled=%u",
             config->schedule_id, config->hour, config->minute,
             config->weekday_mask, config->repeat, config->enabled);
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_stop(void)
{
    esp_err_t first_error = ESP_OK;
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (atomic_load(&s_portal.sta_provisioning)) {
        return ESP_ERR_INVALID_STATE;
    }
    s_portal.running = false;

    portal_dns_server_stop(s_portal.dns_server);
    s_portal.dns_server = NULL;

    if (s_portal.http_server != NULL) {
        esp_err_t err = httpd_stop(s_portal.http_server);
        if ((first_error == ESP_OK) && (err != ESP_OK)) {
            first_error = err;
        }
    }
    if (s_portal.ip_event_handler != NULL) {
        esp_err_t err = esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_portal.ip_event_handler);
        if ((first_error == ESP_OK) && (err != ESP_OK)) {
            first_error = err;
        }
    }
    if (s_portal.wifi_event_handler != NULL) {
        esp_err_t err = esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_portal.wifi_event_handler);
        if ((first_error == ESP_OK) && (err != ESP_OK)) {
            first_error = err;
        }
    }
    if (s_portal.wifi_started) {
        esp_err_t err = esp_wifi_stop();
        if ((first_error == ESP_OK) && (err != ESP_OK)) {
            first_error = err;
        }
    }
    esp_err_t err = esp_wifi_deinit();
    if ((first_error == ESP_OK) && (err != ESP_OK)) {
        first_error = err;
    }
    if (s_portal.ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_portal.ap_netif);
    }
    if (s_portal.sta_netif != NULL) {
        esp_netif_destroy_default_wifi(s_portal.sta_netif);
    }
    if (s_portal.sta_event_group != NULL) {
        vEventGroupDelete(s_portal.sta_event_group);
    }
    vSemaphoreDelete(s_portal.mutex);
    memset(&s_portal, 0, sizeof(s_portal));
    return first_error;
}
