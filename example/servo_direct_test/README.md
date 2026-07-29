# servo_direct_test

ESP32-WROOM direct PWM servo test.

This example does not use CAN or CH32. It drives four servo signal pins from
the output-capable `PWMB` header pins:

- CH0: `PWMB1` / GPIO32
- CH1: `PWMB2` / GPIO33
- CH2: `PWMB3` / GPIO25
- CH3: `PWMB4` / GPIO26

Servo power must come from a suitable external 5 V supply. Connect ESP32 GND
and servo power GND together.

Build:

```powershell
.\tools\build.ps1 example/servo_direct_test esp32 my_board_esp32wroom
```
