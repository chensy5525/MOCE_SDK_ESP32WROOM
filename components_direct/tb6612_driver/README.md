# tb6612_driver

Direct dual-channel TB6612 adapter using the shared GPIO and PWM BSPs.

## Ownership

- `bsp_gpio` and `bsp_pwm` own ESP-IDF peripheral access.
- `tb6612_driver` owns TB6612 direction, duty, initialization, and stop
  semantics.
- The caller owns board resource binding, run duration, acceptable duty, and
  product safety policy.

The handle performs no dynamic allocation. It is single-owner and not
thread-safe; serialize calls externally if multiple FreeRTOS tasks can command
the same driver.

## Safety contract

- Initialization configures both channels with zero PWM and `IN1=IN2=0`.
- Every direction change first writes zero PWM.
- `STOP` accepts only zero duty.
- Deinitialization stops both channels before changing resource ownership.
- With controlled STBY, GPIOs are released only after STBY is driven inactive.
- With external STBY, all PWM and direction pins remain GPIO outputs driven
  low; they are deliberately not released into a potentially floating state.
- STBY control is required by default. `tb6612_driver_config_default()` leaves
  `control_stby=true` and `stby_gpio=GPIO_NUM_NC`, so initialization cannot
  succeed until the caller binds a real STBY GPIO.
- Setting `control_stby=false` is an explicit opt-out for a different board
  whose schematic proves that STBY is held active independently of firmware.
  The driver cannot detect that wiring.

Initialize every handle with `TB6612_DRIVER_INITIALIZER` before calling
`tb6612_driver_init()`.

Command state is software state only. It does not prove rotation, speed,
current, stall status, or direction because there is no feedback input.
