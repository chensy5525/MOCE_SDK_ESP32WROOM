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
#define WIFI_ALARM_WEEKDAY_MONDAY           (1U << 0)
#define WIFI_ALARM_WEEKDAY_TUESDAY          (1U << 1)
#define WIFI_ALARM_WEEKDAY_WEDNESDAY        (1U << 2)
#define WIFI_ALARM_WEEKDAY_THURSDAY         (1U << 3)
#define WIFI_ALARM_WEEKDAY_FRIDAY           (1U << 4)
#define WIFI_ALARM_WEEKDAY_SATURDAY         (1U << 5)
#define WIFI_ALARM_WEEKDAY_SUNDAY           (1U << 6)
#define WIFI_ALARM_WEEKDAY_ALL              0x7FU

typedef struct {
    uint8_t schedule_id;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekday_mask;
    bool repeat;
    bool enabled;
} WifiAlarmConfig;

typedef enum {
    WIFI_ALARM_PORTAL_EVENT_CONFIG_UPDATED = 0,
    WIFI_ALARM_PORTAL_EVENT_TIME_SYNC,
    WIFI_ALARM_PORTAL_EVENT_STOP_CURRENT_RINGING,
} WifiAlarmPortalEventType;

typedef struct {
    WifiAlarmPortalEventType type;
    union {
        WifiAlarmConfig config;
        struct {
            int64_t epoch_seconds;
            int16_t utc_offset_minutes;
        } time_sync;
    } data;
} WifiAlarmPortalEvent;

typedef struct {
    bool clock_synced;
    bool ringing;
    int64_t epoch_seconds;
    int16_t utc_offset_minutes;
} WifiAlarmRuntimeStatus;

/**
 * Both callbacks run in the HTTP server task. They must be bounded and
 * non-blocking. A controller should only copy state or use xQueueSend(..., 0).
 */
typedef esp_err_t (*WifiAlarmPortalEventCallback)(
    const WifiAlarmPortalEvent *event,
    void *user_context);

typedef esp_err_t (*WifiAlarmPortalStatusCallback)(
    WifiAlarmRuntimeStatus *status,
    void *user_context);

/** Legacy configuration callback retained for existing examples. */
typedef esp_err_t (*WifiAlarmApplyCallback)(
    const WifiAlarmConfig *config,
    void *user_context);

typedef struct {
    const char *ap_ssid;
    const char *ap_password;
    /** Optional upstream network. NULL or an empty SSID keeps AP-only mode. */
    const char *sta_ssid;
    const char *sta_password;
    uint8_t sta_maximum_retries;
    uint8_t max_connections;
    WifiAlarmApplyCallback apply_callback;
    WifiAlarmPortalEventCallback event_callback;
    WifiAlarmPortalStatusCallback status_callback;
    void *user_context;
} WifiAlarmPortalOptions;

/** Start SoftAP (and optional STA), NVS-backed alarm configuration, and HTTP server. */
esp_err_t wifi_alarm_portal_start(const WifiAlarmPortalOptions *options);

/** Wait a bounded time for the optional station interface to obtain an IP. */
esp_err_t wifi_alarm_portal_wait_for_sta_ip(uint32_t timeout_ms);

/** Start a new bounded station connection attempt after a previous failure. */
esp_err_t wifi_alarm_portal_reconnect_sta(void);

/** Disable only the station interface; SoftAP/HTTP remain available. */
esp_err_t wifi_alarm_portal_suspend_sta(void);

/** Restore APSTA mode after a station suspension. */
esp_err_t wifi_alarm_portal_resume_sta(void);

/** Copy the current schedule. Debug version supports schedule_id 0 only. */
esp_err_t wifi_alarm_portal_get_config(WifiAlarmConfig *config);

/** Validate, persist, and publish one schedule. */
esp_err_t wifi_alarm_portal_set_config(const WifiAlarmConfig *config);

/** Stop HTTP and Wi-Fi resources owned by this component. */
esp_err_t wifi_alarm_portal_stop(void);

#ifdef __cplusplus
}
#endif

#endif
