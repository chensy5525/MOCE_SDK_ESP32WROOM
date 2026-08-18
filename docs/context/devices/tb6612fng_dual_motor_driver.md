## MOCE_DEVICE_CONTEXT
module_id: tb6612fng_dual_motor_driver
display_name: TB6612FNG dual DC motor driver
category: actuator
electrical_interface:
  bus: gpio_and_pwm
  address_or_pin: two PWM inputs, four direction inputs, and one active-high STBY input
  voltage: not frozen by current controlled evidence
device_semantics:
  init_sequence:
    - command zero PWM and IN1=IN2=0 for both channels
  read_operations:
    - none; no speed, current, position, or fault feedback is connected
  write_operations:
    - set_output: motor A or B, forward or reverse, duty 0..100 percent
    - stop: motor A or B with zero PWM and IN1=IN2=0
  data_conversion:
    - duty percent maps to the configured PWM timer count
capabilities:
  - independent fixed-duty commands for two DC motor channels
  - forward, reverse, and stopped command states
unsupported:
  - measured speed control
  - minimum-starting-duty claims
  - stall detection
  - current limiting feedback
abstract_host_operations_required:
  - pwm_set
  - gpio_set
safe_defaults:
  - initialize and deinitialize both outputs in the stopped state
  - retain PWM and direction pins at low level after deinitialization when STBY is not software-controlled
  - pass through zero PWM before changing direction
user_phrases:
  - drive both motors at a fixed duty
  - stop both motors
forbidden_contamination:
  - servo angle semantics
  - encoder feedback unless a separate encoder device is selected
validation_status: compile_passed
## END_MOCE_DEVICE_CONTEXT
