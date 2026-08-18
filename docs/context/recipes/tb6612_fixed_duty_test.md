## MOCE_RECIPE_CONTEXT
recipe_id: tb6612_fixed_duty_test
purpose: command two TB6612 motor channels forward at one fixed duty for a bounded test interval
supported_boards:
  - my_board_esp32wroom
user_goal:
  - verify fixed-duty motor drive using the desktop pwm_test wiring
selected_devices:
  - tb6612fng_dual_motor_driver
transport_binding:
  transport_context: esp32_native_pwm_gpio_motor
  binding_ids:
    - tb6612_board_v1
node_plan:
  policy: not_applicable
  expected_node_count: 0
required_behavior:
  - initialize both outputs stopped
  - command both channels forward at 70 percent duty and 10 kHz
  - hold the command for 5000 ms
  - stop both outputs and retain all six command pins low because STBY is external
controller_responsibilities:
  - own duty, run duration, error handling, and safe shutdown
  - consume the six board-profile GPIO bindings instead of duplicating pin literals
  - warn that external STBY-high wiring is required
device_adapter_responsibilities:
  - validate resource bindings and duty range
  - force zero PWM before direction changes
  - stop both motors on deinitialization
serial_log:
  - recipe=tb6612_fixed_duty_test
  - selected_devices=tb6612fng_dual_motor_driver
  - transport=esp32_native_pwm_gpio frequency=10000Hz duty=70%
  - both motors commanded forward at fixed duty
  - test complete; both motor commands stopped
state_machine:
  - INIT
  - RUN_FIXED_DUTY
  - STOP
  - COMPLETE
failure_behavior:
  - abort on the first initialization or command error
  - attempt to stop both outputs and hold command pins low
must_not_include:
  - infinite motor drive loop
  - minimum-starting-duty claim
  - feedback-control claim
compile_command: idf.py -C examples_direct/tb6612_fixed_duty_test -B examples_direct/tb6612_fixed_duty_test/build_zsan_review -DMOCE_BOARD=my_board_esp32wroom build
hardware_test_status: refactor_not_tested
## END_MOCE_RECIPE_CONTEXT
