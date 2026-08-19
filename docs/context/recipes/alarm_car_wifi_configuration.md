# Recipe Context: alarm-car Wi-Fi configuration

## Product flow

1. ESP32 starts SoftAP `AlarmCar-06` and the HTTP service.
2. User connects locally and opens `http://192.168.4.1`.
3. User edits hour/minute and enables or disables the alarm.
4. Portal validates the request and asks the alarm controller to accept it.
5. On success, the portal persists the record and publishes the new active state.
6. Separately, the alarm controller compares current time and triggers the audio player.
7. Separately, the audio player reads an approved file from SD, decodes it, drives I2S/amplifier,
   and returns to its safe idle state on completion or failure.

## Module boundary

| Module | Owns | Does not own |
|---|---|---|
| Wi-Fi portal | SoftAP, web/API, validation, NVS, controller handoff | timekeeping, scheduling, SD/audio |
| Alarm controller | current-time source, schedule comparison, trigger policy | HTTP/Wi-Fi, decoding |
| Audio/SD | file access, codec, I2S, amplifier safe state | alarm configuration |

Version 1 configuration is intentionally limited to `hour`, `minute`, and `enabled`. Weekdays,
volume, track selection, NTP, and captive portal behavior require separate contracts.
