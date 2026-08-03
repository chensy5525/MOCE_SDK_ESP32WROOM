# servo_driver

ESP32-WROOM direct four-channel servo PWM driver using LEDC.

## Default Mapping

- CH0: `PWMB1` / GPIO32
- CH1: `PWMB2` / GPIO33
- CH2: `PWMB3` / GPIO25
- CH3: `PWMB4` / GPIO26

The `PWMA` pins on this board map to ESP32 input-only GPIOs, so they are not
used for PWM output.

## Servo PWM

- Frequency: 50 Hz
- Period: 20 ms
- Minimum pulse: 500 us
- Center pulse: 1500 us
- Maximum pulse: 2500 us
