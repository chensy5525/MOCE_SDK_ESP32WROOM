## MOCE_RECIPE_CONTEXT
recipe_id: mg90s_dual_one_shot_five_position_test
purpose: command two MG90S instances through one finite 45-degree-step sweep and finish both at the 90-degree command
supported_boards:
  - my_board_esp32wroom
user_goal:
  - verify that two MG90S servos can be commanded through five fixed nominal positions
selected_devices:
  - mg90s_servo
device_instances:
  - instance_id: mg90s_pwm1
    device_context: mg90s_servo
  - instance_id: mg90s_pwm2
    device_context: mg90s_servo
transport_binding:
  transport_context: esp32_native_pwm_servo
  binding_ids:
    - servo_pwm1_gpio32
    - servo_pwm2_gpio33
node_plan:
  policy: not_applicable
  expected_node_count: 0
required_behavior:
  - command both instances to 0, 45, 90, 135, 180, 135, and 90 degrees
  - hold each command for 3000 ms
  - keep every adjacent command difference at 45 degrees
  - stop switching after both final 90-degree commands
controller_responsibilities:
  - own the finite test sequence
  - bind GPIO32/PWM1 and GPIO33/PWM2 to LEDC timer 1 channels 1 and 2
  - check every driver return code
  - log each commanded position and completion
device_adapter_responsibilities:
  - validate configuration and command values
  - map five position commands to configured pulse widths
  - retain only the last successfully commanded position as software state
serial_log:
  - recipe=mg90s_dual_one_shot_five_position_test
  - selected_devices=mg90s_servo instances=2
  - transport=esp32_native_pwm pwm1_gpio=32 pwm2_gpio=33 frequency=50Hz
  - position=<0|45|90|135|180|135|90> commanded on both channels; observe both servos
  - test complete; both final commands=90; no further switching
state_machine:
  - INIT
  - SWEEP_UP
  - STEP_BACK_TO_90
  - COMPLETE
failure_behavior:
  - abort on the first channel error
  - request the nominal 90-degree safe command for both channels
  - disable both PWM outputs if the group safe-position command fails
must_not_include:
  - infinite position cycling
  - UART commands for position control
  - claims of calibrated mechanical angle accuracy
compile_command: .\tools\build.ps1 examples_direct/servo_direct_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTEXT
