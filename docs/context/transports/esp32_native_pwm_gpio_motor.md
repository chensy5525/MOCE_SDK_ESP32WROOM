## MOCE_TRANSPORT_CONTEXT
transport_id: esp32_native_pwm_gpio_motor
purpose: bind schematic-backed ESP32 PWM and GPIO resources to an external motor driver
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
source_evidence:
  - C:\Users\LENOVO\Desktop\原理图\通用新版\SCH_ESP32-WROOM-32_2026-08-14.pdf
  - C:\Users\LENOVO\Desktop\原理图\通用新版\SCH_ESP32转接板新版_2026-08-14.pdf
  - C:\Users\LENOVO\Desktop\原理图\直流无刷电机TB6612\SCH_直流有刷电机-TB6612_2026-08-14.pdf
resource_bindings:
  - binding_id: tb6612_board_v1
    pwm_a_gpio: 25
    pwm_b_gpio: 26
    ain1_gpio: 27
    ain2_gpio: 14
    bin1_gpio: 12
    bin2_gpio: 13
    stby_gpio: unavailable
    stby_schematic_state: U1_pin_19_unconnected
    stby_connector_state: absent_from_8_pin_connector
    hardware_eligibility: blocked
    pwm_mode: LEDC_HIGH_SPEED_MODE
    pwm_timer: LEDC_TIMER_2
    pwm_channels: LEDC_CHANNEL_3, LEDC_CHANNEL_4
    pwm_resolution: 12_bit
    pwm_frequency_hz: 10000
    share_rules: exclusive_if_enabled; GPIO13 conflicts with SPI CS1
electrical_constraints:
  motor_supply: unknown
  logic_supply: TB6612_module_connector_VCC
  common_ground_required: true
  stby_requirement: deterministic active-high level
  stby_current_state: unsatisfied; U1 pin 19 is unconnected and its internal pull-down selects standby
transport_responsibilities:
  - expose only the six GPIO bindings proven by the three schematics
  - report the missing STBY path as a hardware blocker
  - prevent the board-specific recipe from configuring PWM or direction GPIOs while blocked
  - propagate peripheral errors after a future eligible hardware binding exists
must_not_include:
  - a fabricated STBY GPIO
  - an assumption that external wiring holds STBY high
  - product motion policy
  - claims of motor speed or movement feedback
hardware_validation_status: blocked_by_unconnected_stby
validation_status: compile_passed_hardware_blocked
## END_MOCE_TRANSPORT_CONTEXT
