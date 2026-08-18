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
The driver accepts only the conservative 1000 us to 2000 us command envelope;
values outside that envelope are rejected before LEDC is configured. This is a
software guardrail, not calibrated angle or stall-current evidence.
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
If one channel fails to initialize, the driver attempts to stop every configured
channel and release the timer. If rollback is incomplete, the handle enters the
cleanup-required state so the caller can retry shutdown.

`servo_driver_set_all_positions()` updates channels sequentially because ESP-IDF
LEDC does not provide an atomic multi-channel commit. If one update fails, the
driver immediately commands every channel back to `initial_position`. If that
recovery also fails, it stops every configured PWM output and leaves the handle
in cleanup-required state if shutdown is incomplete. This prevents a failed
coordinated command from silently leaving the other channel unaddressed.

If output shutdown or timer release fails, the handle enters cleanup-required
state. Motion commands are rejected and `servo_driver_deinit()` can be called
again. Calls that share one handle must be serialized by the caller; this
component does not allocate an internal mutex.

All public functions return `esp_err_t`, including nominal-degree conversion.
No sentinel value is used to hide an invalid position. The driver does not
select a product-level safe state; the controller/recipe owns that decision.
