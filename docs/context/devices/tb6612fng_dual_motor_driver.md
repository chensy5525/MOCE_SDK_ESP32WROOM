## MOCE_DEVICE_CONTEXT
module_id: tb6612fng_dual_motor_driver
display_name: TB6612FNG dual DC motor driver
category: actuator
electrical_interface:
  bus: other
  interface_detail: gpio_and_pwm
  address_or_pin: two PWM inputs, four direction inputs, and one active-high STBY input
  voltage: not frozen by current controlled evidence
activation_precondition:
  - STBY must be held at a deterministic high level before either motor channel can operate
  - an unconnected STBY input is not an executable configuration
device_semantics:
  init_sequence:
    - keep STBY inactive while commanding zero PWM and IN1=IN2=0 for both channels
    - activate STBY only after all six command outputs are in a known safe state
  read_operations:
    - none; no speed, current, position, or fault feedback is connected
  write_operations:
    - set_output: motor A or B, forward or reverse, duty 0..100 percent
    - stop: motor A or B with zero PWM and IN1=IN2=0
  data_conversion:
    - duty percent maps to the configured PWM timer count
capabilities:
  - independent fixed-duty commands for two DC motor channels when the activation precondition is satisfied
  - forward, reverse, and stopped command states
unsupported:
  - operation with STBY unconnected, floating, or otherwise not proven high
  - measured speed control
  - minimum-starting-duty claims
  - stall detection
  - current limiting feedback
abstract_host_operations_required:
  - pwm_set
  - gpio_set
safe_defaults:
  - require explicit STBY handling before initialization can succeed
  - initialize and deinitialize both outputs in the stopped state
  - retain PWM and direction pins at low level after deinitialization when STBY is not software-controlled
  - pass through zero PWM before changing direction
user_phrases:
  - drive both motors at a fixed duty
  - stop both motors
forbidden_contamination:
  - board GPIO numbers
  - claims that a requested command proves physical motor movement
  - servo angle semantics
  - encoder feedback unless a separate encoder device is selected
validation_status: compile_passed
## END_MOCE_DEVICE_CONTEXT
