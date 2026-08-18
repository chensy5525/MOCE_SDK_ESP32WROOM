# tb6612_fixed_duty_test

Build-only, fail-closed record of the requested dual-motor fixed-duty test.
The current hardware cannot execute the test because TB6612 U1 pin 19 (STBY)
is unconnected.

## Frozen baseline

- PWM: GPIO25/PWMA and GPIO26/PWMB.
- Direction: GPIO27/AIN1, GPIO14/AIN2, GPIO12/BIN1, GPIO13/BIN2.
- Resource source: `boards/my_board_esp32wroom/board.h`.
- LEDC: high-speed timer 2, channels 3 and 4, 12-bit resolution.
- Frequency: 10 kHz.
- Deferred command: both channels forward at 70% duty for 5 seconds, then
  stop. The application does not issue this command on the current hardware.
- STBY: U1 pin 19 is unconnected, has no external bias, and is absent from the
  8-pin module connector. The TB6612FNG internal pull-down therefore keeps the
  device in standby.

GPIO13 is shared with the SPI CS1 profile. If a future hardware revision makes
STBY deterministically active, the recipe will need exclusive ownership of the
six TB6612 control pins and LEDC timer/channels while running.

## Build

```powershell
idf.py -C examples_direct/tb6612_fixed_duty_test `
  -B examples_direct/tb6612_fixed_duty_test/build_zsan_review `
  -DMOCE_BOARD=my_board_esp32wroom build
```

Run this from an initialized ESP-IDF 6.0.2 shell at the repository root.

## Validation

Successful compilation proves only that the driver component, board constants,
and blocked application compile together. At runtime the application logs the
six schematic-backed bindings and the STBY blocker, then returns without
configuring GPIO or PWM.

Do not flash this image as a motor test and do not mark the recipe board-passed.
Hardware validation can begin only after a revised schematic and matching PCB
provide a deterministic STBY-high path; firmware must not substitute an
invented GPIO.
