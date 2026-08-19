#ifndef WIFI_ALARM_PORTAL_H
#define WIFI_ALARM_PORTAL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_ALARM_PORTAL_SSID_MAX_LEN      32U
#define WIFI_ALARM_PORTAL_PASSWORD_MAX_LEN  63U

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
} WifiAlarmConfig;

/**
 * Called from the HTTP server task. The callback must be bounded, non-blocking,
 * and must not call wifi_alarm_portal_get_config() or stop the portal.
 */
typedef esp_err_t (*WifiAlarmApplyCallback)(const WifiAlarmConfig *config,
                                            void *user_context);

typedef struct {
    const char *ap_ssid;
    const char *ap_password;
    uint8_t max_connections;
    WifiAlarmApplyCallback apply_callback;
    void *user_context;
} WifiAlarmPortalOptions;

/** Start the exclusive ESP32 SoftAP, NVS-backed alarm configuration, and HTTP server. */
esp_err_t wifi_alarm_portal_start(const WifiAlarmPortalOptions *options);

/** Copy the current configuration. Valid only while the portal is running. */
esp_err_t wifi_alarm_portal_get_config(WifiAlarmConfig *config);

/** Stop HTTP and Wi-Fi resources owned by this component. */
esp_err_t wifi_alarm_portal_stop(void);

#ifdef __cplusplus
}
#endif

#endif
