# servo_direct_test

Finite MG90S direct-PWM validation recipe for the ESP32-WROOM board.

## Frozen binding

- Board: ESP32-WROOM-32E-N4 main controller V1.0.
- Signal: `PWM1` / GPIO32.
- LEDC: high-speed mode, timer 1, channel 1, 16-bit resolution.
- PWM: 50 Hz.
- Pulse targets: 1000, 1250, 1500, 1750, and 2000 us.

The sequence is:

```text
0 -> 45 -> 90 -> 135 -> 180 -> 135 -> 90
```

Every command is held for 3 seconds. The task then exits and leaves the final
90-degree PWM command active; it does not cycle forever and does not use serial
input to control the servo.

If a command fails, the recipe first attempts the defined safe state (the
nominal 90-degree command). If that command also fails, it stops the PWM output.

## Wiring and safety

- Connect the servo signal to GPIO32.
- Use an approved 5 V servo supply and connect its ground to ESP32 ground.
- Do not power the servo from the ESP32 3.3 V rail.
- Disconnect servo power on chatter, end-stop stall, overheating, or supply
  collapse.

## Build

```powershell
.\tools\build.ps1 examples_direct/servo_direct_test esp32 my_board_esp32wroom
```

## Validation boundary

The earlier standalone implementation was accepted by the user on hardware on
2026-08-17 with the same GPIO, frequency, pulse targets, and finite sequence.
This refactored BSP-based implementation requires a new hardware regression
before it can be called board-passed. Serial logs prove command execution only;
they do not prove PWM waveform or mechanical angle accuracy.
