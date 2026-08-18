## MOCE_TRANSPORT_CONTEXT
transport_id: esp32_native_pwm_gpio_motor
purpose: provide direct ESP32 PWM and GPIO operations for an external motor driver
supported_host_boards:
  - my_board_esp32wroom
implementation:
  pwm_component: bsp/bsp_pwm
  gpio_component: bsp/bsp_gpio
generic_operations:
  - op: pwm_set
    fields: speed_mode, timer, channel, frequency_hz, duty
  - op: gpio_set
    fields: pin, level
resource_bindings:
  - binding_id: tb6612_board_v1
    pwm_a_gpio: 25
    pwm_b_gpio: 26
    ain1_gpio: 27
    ain2_gpio: 14
    bin1_gpio: 12
    bin2_gpio: 13
    stby_gpio: not_controlled
    pwm_mode: LEDC_HIGH_SPEED_MODE
    pwm_timer: LEDC_TIMER_2
    pwm_channels: LEDC_CHANNEL_3, LEDC_CHANNEL_4
    pwm_resolution: 12_bit
    pwm_frequency_hz: 10000
    share_rules: exclusive; GPIO13 conflicts with SPI CS1
electrical_constraints:
  motor_supply: unknown
  logic_supply: unknown
  common_ground_required: true
  stby_requirement: external circuit must hold STBY active high
transport_responsibilities:
  - configure GPIO and PWM resources
  - propagate peripheral errors
  - hold PWM and direction outputs low after deinitialization when STBY is external
must_not_include:
  - product motion policy
  - claims of motor speed or movement feedback
validation_status: compile_passed
## END_MOCE_TRANSPORT_CONTEXT
