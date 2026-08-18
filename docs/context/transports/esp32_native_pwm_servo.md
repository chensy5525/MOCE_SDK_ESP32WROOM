## MOCE_TRANSPORT_CONTEXT
transport_id: esp32_native_pwm_servo
purpose: generate direct RC-servo PWM through the ESP32-WROOM board PWM header
supported_host_boards:
  - my_board_esp32wroom
implementation:
  component: bsp/bsp_pwm
  public_operations:
    - bsp_pwm_timer_init
    - bsp_pwm_channel_init
    - bsp_pwm_set_duty
    - bsp_pwm_stop
    - bsp_pwm_timer_deinit
generic_operations:
  - op: pwm_set
    fields: speed_mode, timer, channel, frequency_hz, duty
evidence:
  hardware:
    - ESP32-WROOM-32 schematic V1.0, page 1: IO32 is PWM1 and IO33 is PWM2
    - ESP32-WROOM-32 schematic V1.0, page 2: PWM1 and PWM2 are routed through the 18-pin board interface
    - ESP32 adapter board new version schematic V1.0, page 1: the dual-servo connector exposes PWM1, PWM2, and GND
  software_resource_binding:
    - boards/my_board_esp32wroom/board.h owns LEDC mode, timer, channels, resolution, and frequency; these are not schematic claims
resource_bindings:
  - binding_id: servo_pwm1_gpio32
    signal: PWM1
    gpio: 32
    speed_mode: LEDC_HIGH_SPEED_MODE
    timer: LEDC_TIMER_1
    channel: LEDC_CHANNEL_1
    duty_resolution: 16_bit
    frequency_hz: 50
    share_rules: shares timer 1 frequency and resolution with servo_pwm2_gpio33; GPIO and channel are exclusive
  - binding_id: servo_pwm2_gpio33
    signal: PWM2
    gpio: 33
    speed_mode: LEDC_HIGH_SPEED_MODE
    timer: LEDC_TIMER_1
    channel: LEDC_CHANNEL_2
    duty_resolution: 16_bit
    frequency_hz: 50
    share_rules: shares timer 1 frequency and resolution with servo_pwm1_gpio32; GPIO and channel are exclusive
electrical_constraints:
  logic_voltage_v: 3.3
  load_power_source: external servo supply; current capability not verified by this context
  common_ground_required: true
transport_responsibilities:
  - configure and update PWM hardware
  - stop PWM output and release the timer
must_not_include:
  - servo angle semantics
  - product motion sequences
  - claims that a PWM command proves mechanical motion
validation_status: compile_passed
## END_MOCE_TRANSPORT_CONTEXT
