# Wi-Fi alarm portal test

Build-only status is not hardware validation. After explicit flash approval:

1. Flash the ESP32-WROOM-32E-N4 through the assigned CH340 port.
2. Connect a phone or PC to `AlarmCar-06` with the configured password.
3. Open `http://192.168.4.1` manually.
4. Set time and enable state, save, then refresh and power-cycle to verify NVS persistence.
5. Confirm malformed values are rejected and the prior configuration remains active.

The example logs the alarm-controller handoff. It intentionally does not access SD, decode audio,
drive an amplifier, or claim that an alarm actually fired.

`alarmcar06` is a development-only default password. A product build must provision a unique or
owner-configured credential rather than shipping this shared default.
