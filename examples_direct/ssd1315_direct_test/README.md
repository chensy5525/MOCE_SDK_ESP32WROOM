# ssd1315_direct_test

SSD1315 128x64 OLED direct-I2C minimum example for ESP32-WROOM.

- I2C address: `0x3C`
- I2C clock: `400 kHz`
- Default board pins: SDA GPIO21, SCL GPIO22
- Expected display: `SSD1315 OK`
- Expected serial log: `[INF][SSD1315] init OK, addr=0x3C`
- Refresh behavior: only dirty pages are sent; one public refresh has a 3000 ms
  total deadline and each I2C transaction is capped at 1000 ms
- Lifecycle: after the text becomes visible, the example keeps the driver
  initialized for 60 seconds, then deinitializes and returns

Build from the repository root:

```powershell
.\tools\build.ps1 examples_direct/ssd1315_direct_test esp32 my_board_esp32wroom
```
