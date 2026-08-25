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
    - set_angle: integer target command from 0 through 180 degrees
    - set_position: compatibility command in 0, 45, 90, 135, or 180 degrees
  data_conversion:
    - integer angles use piecewise-linear pulse interpolation between configurable 0,45,90,135,180-degree calibration points
    - current default calibration maps 0,45,90,135,180 degrees to 500,1000,1500,2000,2500 us
capabilities:
  - command any integer target angle from 0 through 180 degrees
  - command one selected channel or all configured channels
unsupported:
  - calibrated mechanical angle accuracy without per-device measurement
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
  - set either servo to any integer angle from 0 to 180 degrees
  - move the MG90S to a fixed position
forbidden_contamination:
  - motor speed-control semantics
  - continuous-rotation claims
  - serial-control requirements
validation_status: untested
## END_MOCE_DEVICE_CONTEXT
