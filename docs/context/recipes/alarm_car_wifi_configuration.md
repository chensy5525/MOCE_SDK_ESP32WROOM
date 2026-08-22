# Recipe Context: alarm-car Wi-Fi configuration

## Product flow

1. ESP32 starts the configured SoftAP and local HTTP/DNS service.
2. User connects locally and opens `http://192.168.4.1`.
3. User scans or manually enters an upstream SSID/password. The portal tests the candidate in RAM,
   persists it only after an IP is obtained, and rolls back on connection or NVS failure.
4. The network-time task exists even without build-time credentials. After dynamic provisioning
   obtains an IP it starts SNTP on the next bounded check (normally within one second). SNTP supplies
   the authoritative time; phone HTTP time is not accepted by the current controller.
5. User edits time, weekdays, repeat and enabled state for debug schedule `0`.
6. Portal validates, persists and publishes the alarm record.
7. Alarm controller compares current local time and emits trigger/stop hooks.

## Module boundary

| Module | Owns | Does not own |
|---|---|---|
| Wi-Fi portal | SoftAP/APSTA, web/API, scan, credential and alarm NVS, controller handoff | SNTP acceptance, scheduling, audio/motion |
| Alarm controller | current-time source, schedule comparison, trigger policy | HTTP/Wi-Fi, decoding |
| Audio/SD | file access, codec, I2S, amplifier safe state | alarm configuration |

The debug configuration is limited to one schedule. Volume, track selection, multiple schedules,
per-device authentication and guaranteed captive-page behavior require separate contracts.
