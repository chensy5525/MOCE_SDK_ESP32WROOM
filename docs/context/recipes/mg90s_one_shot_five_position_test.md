## MOCE_RECIPE_CONTEXT
recipe_id: mg90s_one_shot_five_position_test
purpose: execute one finite 45-degree-step MG90S sweep and finish at the 90-degree command
supported_boards:
  - my_board_esp32wroom
user_goal:
  - verify that the MG90S can switch among five fixed nominal positions
selected_devices:
  - mg90s_servo
transport_binding:
  transport_context: esp32_native_pwm_servo
  binding_ids:
    - servo_pwm1_gpio32
node_plan:
  policy: not_applicable
  expected_node_count: 0
required_behavior:
  - execute 0, 45, 90, 135, 180, 135, and 90 degree commands
  - hold each command for 3000 ms
  - keep every adjacent command difference at 45 degrees
  - stop switching after the final 90-degree command
controller_responsibilities:
  - own the finite test sequence
  - bind GPIO32 and the LEDC resources
  - check every driver return code
  - log each commanded position and completion
device_adapter_responsibilities:
  - validate configuration and command values
  - map five position commands to configured pulse widths
  - retain only the last successfully commanded position as software state
serial_log:
  - recipe=mg90s_one_shot_five_position_test
  - selected_devices=mg90s_servo
  - transport=esp32_native_pwm gpio=32 frequency=50Hz
  - position=<0|45|90|135|180|135|90> commanded; observe the servo
  - test complete; final command=90; no further switching
state_machine:
  - INIT
  - SWEEP_UP
  - STEP_BACK_TO_90
  - COMPLETE
failure_behavior:
  - abort on the first driver error
  - request the nominal 90-degree safe command
  - disable PWM if the safe-position command also fails
must_not_include:
  - infinite position cycling
  - UART commands for position control
  - claims of calibrated mechanical angle accuracy
compile_command: .\tools\build.ps1 examples_direct/servo_direct_test esp32 my_board_esp32wroom
hardware_test_status: refactor_not_retested
## END_MOCE_RECIPE_CONTEXT
