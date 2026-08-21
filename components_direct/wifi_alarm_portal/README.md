# Wi-Fi alarm portal

ESP32 native SoftAP + local HTTP configuration service for alarm-car integration.

Owned here:

- SoftAP lifecycle and local page at `http://192.168.4.1`
- wildcard DNS and OS connectivity-probe redirects for captive-portal discovery
- bounded validation of schedule ID, time, weekday mask, repeat, and enabled state
- versioned NVS persistence in namespace `alarm06`
- non-blocking event/status callbacks into an alarm controller
- strict JSON API for time sync, schedule update, status, and stopping one ringing event

Not owned here:

- current-time/RTC maintenance and alarm scheduling
- SD-card filesystem, audio decoding, I2S, amplifier control, or speaker safety
- device-specific guarantees that every phone OS version will auto-open the page

The component exclusively owns the ESP-IDF Wi-Fi driver while running. Callbacks run in the HTTP
server task and must be bounded, non-blocking, and must not re-enter the portal API.

The persisted `alarm06/config` record is versioned. Existing v1 records are migrated to v2 without
erasing NVS. The current debug contract supports `schedule_id=0` and uses `bit0=Monday` through
`bit6=Sunday`.
