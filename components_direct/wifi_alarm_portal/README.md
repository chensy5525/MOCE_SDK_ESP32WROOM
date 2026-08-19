# Wi-Fi alarm portal

ESP32 native SoftAP + local HTTP configuration service for alarm-car integration.

Owned here:

- SoftAP lifecycle and local page at `http://192.168.4.1`
- bounded validation of `hour`, `minute`, and `enabled`
- versioned NVS persistence in namespace `alarm06`
- non-blocking callback into an alarm controller

Not owned here:

- current-time/RTC maintenance and alarm scheduling
- SD-card filesystem, audio decoding, I2S, amplifier control, or speaker safety
- captive-portal DNS/OS auto-popup behavior

The component exclusively owns the ESP-IDF Wi-Fi driver while running. The apply callback runs in
the HTTP server task and must not block or re-enter the portal API.
