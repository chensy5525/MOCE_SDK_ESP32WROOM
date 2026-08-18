# tb6612_fixed_duty_test

Finite dual-motor fixed-duty test derived from the standalone desktop
`pwm_test` implementation.

## Frozen baseline

- PWM: GPIO25/PWMA and GPIO26/PWMB.
- Direction: GPIO27/AIN1, GPIO14/AIN2, GPIO12/BIN1, GPIO13/BIN2.
- Resource source: `boards/my_board_esp32wroom/board.h`.
- LEDC: high-speed timer 2, channels 3 and 4, 12-bit resolution.
- Frequency: 10 kHz.
- Command: both channels forward at 70% duty for 5 seconds, then stop.
- STBY: not controlled by ESP32; external active-high wiring is required.

GPIO13 is shared with the SPI CS1 profile. The recipe exclusively owns the six
TB6612 control pins and LEDC timer/channels while running.

## Build

```powershell
idf.py -C examples_direct/tb6612_fixed_duty_test `
  -B examples_direct/tb6612_fixed_duty_test/build_zsan_review `
  -DMOCE_BOARD=my_board_esp32wroom build
```

Run this from an initialized ESP-IDF 6.0.2 shell at the repository root.

## Validation

The desktop project contains a compiled fixed-duty implementation using the
same electrical binding. That evidence does not prove this refactored source on
hardware. Verify STBY voltage, motor supply, common ground, PWM waveforms, motor
rotation, low-level hold after deinitialization, and stop behavior before
marking board-passed.
