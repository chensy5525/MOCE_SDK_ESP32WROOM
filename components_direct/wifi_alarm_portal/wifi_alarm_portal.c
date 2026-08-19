#include "wifi_alarm_portal.h"

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

#define ALARM_NVS_NAMESPACE       "alarm06"
#define ALARM_NVS_KEY             "config"
#define ALARM_RECORD_MAGIC        0x41303643UL
#define ALARM_RECORD_VERSION      1U
#define ALARM_DEFAULT_HOUR        7U
#define ALARM_DEFAULT_MINUTE      0U
#define ALARM_HTTP_BODY_MAX_LEN   96U
#define ALARM_HTTP_RESPONSE_LEN   96U
#define ALARM_MUTEX_TIMEOUT_MS    1000U
#define ALARM_AP_MAX_CONNECTIONS  4U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
    uint8_t reserved[3];
    uint32_t checksum;
} AlarmNvsRecord;

typedef struct {
    bool running;
    bool wifi_started;
    esp_netif_t *ap_netif;
    httpd_handle_t http_server;
    SemaphoreHandle_t mutex;
    WifiAlarmConfig alarm;
    WifiAlarmApplyCallback apply_callback;
    void *user_context;
} WifiAlarmPortalState;

static const char *TAG = "wifi_alarm_portal";
static WifiAlarmPortalState s_portal;

static const char ALARM_PAGE[] =
    "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>闹钟小车</title><style>body{font-family:sans-serif;max-width:420px;"
    "margin:40px auto;padding:0 20px}label{display:block;margin:16px 0}"
    "input,button{font-size:18px;padding:8px}#msg{margin-top:16px}</style></head>"
    "<body><h1>闹钟小车</h1><form id='f'><label>时间 <input id='time' type='time'"
    " required></label><label><input id='enabled' type='checkbox'> 启用闹钟</label>"
    "<button type='submit'>保存</button></form><div id='msg'></div><script>"
    "const form=document.getElementById('f'),clock=document.getElementById('time'),"
    "toggle=document.getElementById('enabled'),msg=document.getElementById('msg');"
    "fetch('/api/alarm').then(r=>r.json()).then(v=>{clock.value=String(v.hour)"
    ".padStart(2,'0')+':' +String(v.minute).padStart(2,'0');toggle.checked=v.enabled;})"
    ".catch(()=>msg.textContent='读取失败');form.onsubmit=async e=>{e.preventDefault();"
    "const p=clock.value.split(':');const b=new URLSearchParams({hour:p[0],minute:p[1],"
    "enabled:toggle.checked?'1':'0'});"
    "const r=await fetch('/api/alarm',{method:'POST',headers:{'Content-Type':"
    "'application/x-www-form-urlencoded'},body:b});msg.textContent=await r.text();};"
    "</script></body></html>";

static uint32_t alarm_record_checksum(const AlarmNvsRecord *record)
{
    return record->magic ^ record->version ^ record->hour ^ record->minute ^
           record->enabled ^ 0x5A17C3E9UL;
}

static bool alarm_config_is_valid(const WifiAlarmConfig *config)
{
    return (config != NULL) && (config->hour < 24U) && (config->minute < 60U);
}

static esp_err_t alarm_lock(void)
{
    if ((s_portal.mutex == NULL) ||
        (xSemaphoreTake(s_portal.mutex, pdMS_TO_TICKS(ALARM_MUTEX_TIMEOUT_MS)) != pdTRUE)) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void alarm_unlock(void)
{
    xSemaphoreGive(s_portal.mutex);
}

static esp_err_t alarm_nvs_load(WifiAlarmConfig *config)
{
    nvs_handle_t handle;
    AlarmNvsRecord record = {0};
    size_t record_size = sizeof(record);
    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        config->hour = ALARM_DEFAULT_HOUR;
        config->minute = ALARM_DEFAULT_MINUTE;
        config->enabled = false;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open NVS for read failed");

    err = nvs_get_blob(handle, ALARM_NVS_KEY, &record, &record_size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        config->hour = ALARM_DEFAULT_HOUR;
        config->minute = ALARM_DEFAULT_MINUTE;
        config->enabled = false;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "read alarm record failed");

    if ((record_size != sizeof(record)) || (record.magic != ALARM_RECORD_MAGIC) ||
        (record.version != ALARM_RECORD_VERSION) ||
        (record.checksum != alarm_record_checksum(&record))) {
        return ESP_ERR_INVALID_CRC;
    }

    config->hour = record.hour;
    config->minute = record.minute;
    config->enabled = (record.enabled != 0U);
    return alarm_config_is_valid(config) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t alarm_nvs_save(const WifiAlarmConfig *config)
{
    nvs_handle_t handle;
    AlarmNvsRecord record = {
        .magic = ALARM_RECORD_MAGIC,
        .version = ALARM_RECORD_VERSION,
        .hour = config->hour,
        .minute = config->minute,
        .enabled = config->enabled ? 1U : 0U,
    };
    record.checksum = alarm_record_checksum(&record);

    esp_err_t err = nvs_open(ALARM_NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "open NVS for write failed");
    err = nvs_set_blob(handle, ALARM_NVS_KEY, &record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t parse_u8_field(const char *body, const char *key,
                                uint8_t max_value, uint8_t *value)
{
    char text[4] = {0};
    uint16_t parsed = 0U;

    if (httpd_query_key_value(body, key, text, sizeof(text)) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t length = strnlen(text, sizeof(text));
    if ((length == 0U) || (length >= sizeof(text))) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t index = 0U; index < length; ++index) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return ESP_ERR_INVALID_ARG;
        }
        parsed = (uint16_t)((parsed * 10U) + (uint16_t)(text[index] - '0'));
    }
    if (parsed > max_value) {
        return ESP_ERR_INVALID_ARG;
    }
    *value = (uint8_t)parsed;
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, ALARM_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t alarm_get_handler(httpd_req_t *request)
{
    char response[ALARM_HTTP_RESPONSE_LEN];
    WifiAlarmConfig config;
    esp_err_t err = wifi_alarm_portal_get_config(&config);
    if (err != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "state unavailable");
    }

    int length = snprintf(response, sizeof(response),
                          "{\"hour\":%u,\"minute\":%u,\"enabled\":%s}",
                          config.hour, config.minute, config.enabled ? "true" : "false");
    if ((length < 0) || ((size_t)length >= sizeof(response))) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "encode failed");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response, length);
}

static esp_err_t receive_bounded_body(httpd_req_t *request, char *body, size_t body_size)
{
    if ((request->content_len <= 0) || ((size_t)request->content_len >= body_size)) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t received = 0U;
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(request, body + received,
                                    (size_t)request->content_len - received);
        if (result <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)result;
    }
    body[received] = '\0';
    return ESP_OK;
}

static esp_err_t alarm_post_handler(httpd_req_t *request)
{
    char body[ALARM_HTTP_BODY_MAX_LEN + 1U] = {0};
    WifiAlarmConfig candidate = {0};
    WifiAlarmConfig previous;
    uint8_t enabled;
    esp_err_t err = receive_bounded_body(request, body, sizeof(body));
    if ((err != ESP_OK) ||
        (parse_u8_field(body, "hour", 23U, &candidate.hour) != ESP_OK) ||
        (parse_u8_field(body, "minute", 59U, &candidate.minute) != ESP_OK) ||
        (parse_u8_field(body, "enabled", 1U, &enabled) != ESP_OK)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid alarm config");
    }
    candidate.enabled = (enabled == 1U);

    err = alarm_lock();
    if (err != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "state busy");
    }
    previous = s_portal.alarm;

    if (s_portal.apply_callback != NULL) {
        err = s_portal.apply_callback(&candidate, s_portal.user_context);
    }
    if (err == ESP_OK) {
        err = alarm_nvs_save(&candidate);
    }
    if (err == ESP_OK) {
        s_portal.alarm = candidate;
    } else if (s_portal.apply_callback != NULL) {
        esp_err_t rollback_err = s_portal.apply_callback(&previous, s_portal.user_context);
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "alarm rollback failed: %s", esp_err_to_name(rollback_err));
        }
    }
    alarm_unlock();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "alarm update failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    ESP_LOGI(TAG, "alarm updated: %02u:%02u enabled=%u",
             candidate.hour, candidate.minute, candidate.enabled);
    return httpd_resp_sendstr(request, "保存成功");
}

static esp_err_t alarm_http_start(void)
{
    esp_err_t ret;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler,
    };
    const httpd_uri_t get_alarm = {
        .uri = "/api/alarm", .method = HTTP_GET, .handler = alarm_get_handler,
    };
    const httpd_uri_t post_alarm = {
        .uri = "/api/alarm", .method = HTTP_POST, .handler = alarm_post_handler,
    };

    ESP_RETURN_ON_ERROR(httpd_start(&s_portal.http_server, &config), TAG,
                        "HTTP server start failed");
    ESP_GOTO_ON_ERROR(httpd_register_uri_handler(s_portal.http_server, &root), fail, TAG,
                      "register root failed");
    ESP_GOTO_ON_ERROR(httpd_register_uri_handler(s_portal.http_server, &get_alarm), fail, TAG,
                      "register alarm GET failed");
    ESP_GOTO_ON_ERROR(httpd_register_uri_handler(s_portal.http_server, &post_alarm), fail, TAG,
                      "register alarm POST failed");
    return ESP_OK;

fail:
    httpd_stop(s_portal.http_server);
    s_portal.http_server = NULL;
    return ret;
}

static esp_err_t validate_options(const WifiAlarmPortalOptions *options)
{
    if ((options == NULL) || (options->ap_ssid == NULL) ||
        (options->ap_password == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t ssid_len = strnlen(options->ap_ssid, WIFI_ALARM_PORTAL_SSID_MAX_LEN + 1U);
    size_t password_len = strnlen(options->ap_password,
                                  WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN + 1U);
    if ((ssid_len == 0U) || (ssid_len > WIFI_ALARM_PORTAL_SSID_MAX_LEN) ||
        (password_len > WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN) ||
        ((password_len > 0U) && (password_len < 8U)) ||
        (options->max_connections == 0U) ||
        (options->max_connections > ALARM_AP_MAX_CONNECTIONS)) {
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
    s_portal.apply_callback = options->apply_callback;
    s_portal.user_context = options->user_context;

    ret = nvs_flash_init();
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "NVS init failed; storage was not erased");
    ESP_GOTO_ON_ERROR(alarm_nvs_load(&s_portal.alarm), fail, TAG, "alarm load failed");
    if (s_portal.apply_callback != NULL) {
        ESP_GOTO_ON_ERROR(s_portal.apply_callback(&s_portal.alarm, s_portal.user_context),
                          fail, TAG, "initial alarm apply failed");
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

    ESP_GOTO_ON_ERROR(esp_wifi_init(&wifi_init), fail, TAG, "Wi-Fi init failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), fail_wifi, TAG,
                      "Wi-Fi storage setup failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), fail_wifi, TAG,
                      "Wi-Fi mode setup failed");

    size_t ssid_len = strlen(options->ap_ssid);
    size_t password_len = strlen(options->ap_password);
    memcpy(wifi_config.ap.ssid, options->ap_ssid, ssid_len);
    wifi_config.ap.ssid_len = (uint8_t)ssid_len;
    memcpy(wifi_config.ap.password, options->ap_password, password_len);
    wifi_config.ap.channel = 1U;
    wifi_config.ap.max_connection = options->max_connections;
    wifi_config.ap.authmode = (password_len == 0U) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.pmf_cfg.required = false;

    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), fail_wifi, TAG,
                      "Wi-Fi AP config failed");
    ESP_GOTO_ON_ERROR(esp_wifi_start(), fail_wifi, TAG, "Wi-Fi start failed");
    s_portal.wifi_started = true;
    ESP_GOTO_ON_ERROR(alarm_http_start(), fail_wifi, TAG, "portal HTTP start failed");

    s_portal.running = true;
    ESP_LOGI(TAG, "SoftAP '%s' ready; open http://192.168.4.1", options->ap_ssid);
    return ESP_OK;

fail_wifi:
    if (s_portal.wifi_started) {
        esp_wifi_stop();
        s_portal.wifi_started = false;
    }
    esp_wifi_deinit();
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

esp_err_t wifi_alarm_portal_stop(void)
{
    esp_err_t first_error = ESP_OK;
    if (!s_portal.running) {
        return ESP_ERR_INVALID_STATE;
    }
    s_portal.running = false;

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
