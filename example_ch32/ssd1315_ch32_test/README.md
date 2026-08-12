# ssd1315_ch32_test

SSD1315 128x64 OLED dynamic CH32-I2C gateway example.

The example initializes the repository's shared CH32 CAN gateway, performs
incremental discovery, merges nodes by stable token, scans every confirmed
gateway's downstream I2C bus, and creates one OLED instance for each stable
node containing address `0x3C`.

The SSD1315 driver retains a pointer to the stable node-table entry instead of
copying a runtime node ID. If rediscovery changes `node_id`, the stable entry is
updated in place and the existing display handle follows the new route.

Expected display: `SSD1315 OK`.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_ch32/ssd1315_ch32_test esp32 my_board_esp32wroom
```
