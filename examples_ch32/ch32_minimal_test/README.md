# ch32_minimal_test

Smallest ESP32-WROOM test for CH32 CAN gateway nodes.

This example only listens on CAN. It does not send peripheral commands, so it
can be used with I2C, motor, servo, or other CH32 gateway firmware as long as
the CH32 sends the common `0x700 + NODE_ID` HELLO / HEARTBEAT frame.

Build:

```powershell
.\tools\build.ps1 examples_ch32/ch32_minimal_test esp32 my_board_esp32wroom
```

Expected serial output:

```text
hello node=1 type=I2C fw=1 cap=0x01
ch32 nodes: [1 I2C fw=1 cap=0x01]
```
