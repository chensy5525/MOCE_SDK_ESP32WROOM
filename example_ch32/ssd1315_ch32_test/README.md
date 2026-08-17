# ssd1315_ch32_test

SSD1315 128x64 OLED dynamic CH32-I2C gateway example.

The example initializes the repository's shared CH32 CAN gateway, performs
incremental discovery into one persistent stable-node table, then passes the
confirmed candidates to the SSD1315 driver's authoritative live probe. It
retains only the first OLED instance that initializes successfully at `0x3C`.

The SSD1315 driver retains a pointer to the stable node-table entry instead of
copying a runtime node ID. Discovery is retried every 10 seconds only while no
display has been bound; it stops after the expected display is visible.

Expected display: `SSD1315 OK`. Once visible, the example retains the display
instance for 60 seconds, then deinitializes it and returns.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_ch32/ssd1315_ch32_test esp32 my_board_esp32wroom
```
