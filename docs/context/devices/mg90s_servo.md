## MOCE_DEVICE_CONTEXT
module_id: mg90s_servo
display_name: MG90S positional servo
category: actuator
electrical_interface:
  bus: pwm
  address_or_pin: one PWM signal plus servo power and common ground
  signal_voltage: 3.3 V host PWM used in the accepted baseline test
  supply_voltage: not frozen by the current controlled evidence
device_semantics:
  init_sequence:
    - apply the configured nominal 90-degree pulse
  read_operations:
    - none; no host-readable position or fault feedback is available
  write_operations:
    - set_position: nominal command in 0, 45, 90, 135, or 180 degrees
  data_conversion:
    - 0,45,90,135,180 commands map to configurable PWM pulse targets
capabilities:
  - switch among five nominal position commands
unsupported:
  - arbitrary continuous-angle accuracy
  - measured position feedback
  - arrival confirmation
  - current, temperature, or stall feedback
abstract_host_operations_required:
  - pwm_set
safe_defaults:
  - initialize with the nominal 90-degree pulse
  - a multi-servo recipe failure should request the nominal 90-degree command for every selected instance
  - disable every selected PWM output if the group safe-position command cannot be applied
user_phrases:
  - switch the servo angle in 45-degree steps
  - move the MG90S to a fixed position
forbidden_contamination:
  - motor speed-control semantics
  - continuous-rotation claims
  - serial-control requirements
validation_status: compile_passed
## END_MOCE_DEVICE_CONTEXT
