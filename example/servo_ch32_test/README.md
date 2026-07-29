# servo_ch32_test

Minimal ESP32-WROOM example for the fixed CH32 servo gateway.

The ESP32-WROOM does not generate servo PWM. It only:

- waits for a CH32 servo gateway HELLO frame
- sends documented CAN servo angle commands
- checks `0x500 + NODE_ID` ACK frames
- prints serial status

Build:

```powershell
.\tools\build.ps1 example/servo_ch32_test esp32 my_board_esp32wroom
```
