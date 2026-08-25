# servo_direct_test

Finite dual-MG90S arbitrary-angle direct-PWM validation recipe for the
ESP32-WROOM board.

## Frozen binding

- Board: ESP32-WROOM-32E-N4 main controller V1.0.
- Servo 1 signal: `PWM1` / GPIO32 / LEDC high-speed timer 1 channel 1.
- Servo 2 signal: `PWM2` / GPIO33 / LEDC high-speed timer 1 channel 2.
- Both channels use the shared 50 Hz timer at 16-bit resolution.
- PWM: 50 Hz.
- Calibration targets: 0/45/90/135/180 degrees map to
  500/1000/1500/2000/2500 us.
- Intermediate integer angles use piecewise-linear interpolation.

The sequence is:

```text
0 -> 17 -> 63 -> 91 -> 127 -> 180 -> 90
```

The non-45-degree values exercise the arbitrary-angle API. Both channels
receive every command. Each command is held for 3 seconds. The
task then exits and leaves both final 90-degree PWM commands active; it does not
cycle forever and does not use serial input to control either servo.

If either channel command fails, the driver addresses the pair as one safety
group: it first restores both channels to the nominal 90-degree command. If
that recovery fails, it stops both PWM outputs. Initialization failure also
rolls back the whole pair; incomplete shutdown remains explicit in the log.

## Wiring and safety

- Connect servo 1 signal to PWM1/GPIO32 and servo 2 signal to PWM2/GPIO33.
- Use a servo supply approved for the combined load and connect its ground to
  ESP32 ground; the current capability has not been validated by this task.
- Do not power either servo from the ESP32 3.3 V rail.
- Disconnect both servos' power on chatter, end-stop stall, overheating, or supply
  collapse.

## Build

```powershell
.\tools\build.ps1 examples_direct/servo_direct_test esp32 my_board_esp32wroom
```

## Validation boundary

The earlier standalone evidence covers only one servo on PWM1/GPIO32 and does
not validate this dual-channel source. The current dual-channel implementation
is hardware-untested and requires a new regression before it can be called
board-passed. Serial logs prove command execution only; they do not prove both
PWM waveforms, supply integrity, or mechanical angle accuracy. Verify
intermediate pulse widths with a scope or logic analyzer before accepting the
interpolation path on hardware.
