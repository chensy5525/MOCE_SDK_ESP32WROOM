#include "wifi_alarm_portal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portal_dns_server.h"

#define ALARM_NVS_NAMESPACE        "alarm06"
#define ALARM_NVS_KEY              "config"
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
#define ALARM_MIN_EPOCH_SECONDS    INT64_C(1577836800)
#define ALARM_MAX_EPOCH_SECONDS    INT64_C(9007199254740991)
#define ALARM_MIN_UTC_OFFSET_MIN   (-720)
#define ALARM_MAX_UTC_OFFSET_MIN   840
#define PORTAL_HTTP_STATUS_SERVICE_UNAVAILABLE "503 Service Unavailable"

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
    bool running;
    bool wifi_started;
    esp_netif_t *ap_netif;
    httpd_handle_t http_server;
    PortalDnsServer *dns_server;
    SemaphoreHandle_t mutex;
    WifiAlarmConfig alarm;
    WifiAlarmPortalOptions options;
} WifiAlarmPortalState;

static const char *TAG = "wifi_alarm_portal";
static WifiAlarmPortalState s_portal;

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
    config.max_uri_handlers = 10U;
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
    return ESP_OK;
}

esp_err_t wifi_alarm_portal_start(const WifiAlarmPortalOptions *options)
{
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};
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

    ret = nvs_flash_init();
    ESP_GOTO_ON_ERROR(ret, fail, TAG,
                      "NVS init failed; storage was not erased");
    ESP_GOTO_ON_ERROR(alarm_nvs_load(&s_portal.alarm), fail, TAG,
                      "alarm load failed");

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

    ESP_GOTO_ON_ERROR(esp_wifi_init(&wifi_init), fail, TAG,
                      "Wi-Fi init failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM),
                      fail_wifi, TAG, "Wi-Fi storage setup failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP),
                      fail_wifi, TAG, "Wi-Fi mode setup failed");

    size_t ssid_len = strlen(options->ap_ssid);
    size_t password_len = strlen(options->ap_password);
    memcpy(wifi_config.ap.ssid, options->ap_ssid, ssid_len);
    wifi_config.ap.ssid_len = (uint8_t)ssid_len;
    memcpy(wifi_config.ap.password, options->ap_password, password_len);
    wifi_config.ap.channel = 1U;
    wifi_config.ap.max_connection = options->max_connections;
    wifi_config.ap.authmode = (password_len == 0U) ?
        WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.pmf_cfg.required = false;

    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config),
                      fail_wifi, TAG, "Wi-Fi AP config failed");
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
    (void)esp_wifi_deinit();
fail:
    if (s_portal.ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_portal.ap_netif);
    }
    vSemaphoreDelete(s_portal.mutex);
    memset(&s_portal, 0, sizeof(s_portal));
    return ret;
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
    s_portal.running = false;

    portal_dns_server_stop(s_portal.dns_server);
    s_portal.dns_server = NULL;

    if (s_portal.http_server != NULL) {
        esp_err_t err = httpd_stop(s_portal.http_server);
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
    vSemaphoreDelete(s_portal.mutex);
    memset(&s_portal, 0, sizeof(s_portal));
    return first_error;
}
