# motor_ch32_test

Minimal ESP32-WROOM example for the fixed CH32 motor gateway.

The ESP32-WROOM does not generate motor PWM. It only:

- waits for a CH32 motor gateway HELLO frame
- sends documented CAN motor duty commands
- checks `0x500 + NODE_ID` ACK frames
- prints serial status

Build:

```powershell
.\tools\build.ps1 examples_ch32/motor_ch32_test esp32 my_board_esp32wroom
```
