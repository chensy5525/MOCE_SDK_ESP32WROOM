#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "wifi_alarm_portal.h"

#define PORTAL_MAX_CONNECTIONS 4U
#define PORTAL_IDLE_PERIOD_MS  1000U

static const char *TAG = "wifi_alarm_test";

static esp_err_t alarm_apply(const WifiAlarmConfig *config, void *user_context)
{
    (void)user_context;
    ESP_LOGI(TAG, "controller handoff: %02u:%02u enabled=%u",
             config->hour, config->minute, config->enabled);
    return ESP_OK;
}

void app_main(void)
{
    const WifiAlarmPortalOptions options = {
        .ap_ssid = CONFIG_ALARM_PORTAL_AP_SSID,
        .ap_password = CONFIG_ALARM_PORTAL_AP_PASSWORD,
        .max_connections = PORTAL_MAX_CONNECTIONS,
        .apply_callback = alarm_apply,
        .user_context = NULL,
    };

    esp_err_t err = wifi_alarm_portal_start(&options);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "portal start failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "connect to '%s', then open http://192.168.4.1", options.ap_ssid);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(PORTAL_IDLE_PERIOD_MS));
    }
}
