# servo_driver

Direct PWM adapter for standard positional servos on ESP32-WROOM.

## Ownership

- `bsp_pwm` owns ESP-IDF LEDC access.
- `servo_driver` owns angle-command semantics, pulse validation, channel state,
  and initialization rollback.
- The caller owns GPIO/timer/channel resource binding and motion sequences.

The component performs no dynamic allocation and stores channel configuration
inside each `ServoDriver` handle. Multiple handles are allowed only when their
LEDC timers, channels, and GPIOs do not conflict; the resource planner must
enforce that system-level rule.

## Supported command contract

The public command set is limited to five nominal positions:

| Command | Default pulse width |
|---:|---:|
| 0 degrees | 1000 us |
| 45 degrees | 1250 us |
| 90 degrees | 1500 us |
| 135 degrees | 1750 us |
| 180 degrees | 2000 us |

PWM frequency is fixed at 50 Hz. Pulse widths are configurable because real
mechanical angle depends on the servo, supply, load, linkage, and calibration.
`servo_driver_get_commanded_position()` returns the last accepted command; it is
not position feedback.

## Lifecycle

1. Initialize `ServoDriver` with `SERVO_DRIVER_INITIALIZER` and zero-initialize
   `ServoDriverConfig`.
2. Call `servo_driver_config_default()`.
3. Bind one to four output-capable GPIOs and unique LEDC channels.
4. Call `servo_driver_init()`.
5. Use `servo_driver_set_position()` or `servo_driver_set_all_positions()`.
6. Call `servo_driver_deinit()` when the output should be released.

Initialization starts every configured channel at the nominal 90-degree pulse.
If one channel fails to initialize, earlier channels are stopped and the timer
is released. A multi-channel command can partially succeed; callers requiring
coordinated motion must handle that explicitly.

All public functions return `esp_err_t`, including nominal-degree conversion.
No sentinel value is used to hide an invalid position. The driver does not
select a product-level safe state; the controller/recipe owns that decision.
