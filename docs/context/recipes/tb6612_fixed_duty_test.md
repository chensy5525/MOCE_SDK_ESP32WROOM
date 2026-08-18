## MOCE_RECIPE_CONTEXT
recipe_id: tb6612_fixed_duty_test
purpose: preserve the requested fixed-duty test parameters while failing closed on the current STBY hardware blocker
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
execution_eligibility: blocked
blocking_fact:
  - TB6612 module U1 pin 19 STBY is unconnected, has no external bias, and is absent from the 8-pin connector
deferred_behavior_after_hardware_revision:
  - initialize both outputs stopped
  - command both channels forward at 70 percent duty and 10 kHz
  - hold the command for 5000 ms
  - stop both outputs safely
required_behavior:
  - log the recipe identifier and six schematic-backed GPIO bindings
  - log hardware_blocked=TB6612_U1_STBY_pin_19_unconnected
  - return without initializing the TB6612 driver
  - do not configure PWM or direction GPIOs
  - do not issue any motor command
esp32_responsibilities:
  - enforce the transport eligibility gate before device initialization
  - own future duty, run duration, error handling, and safe shutdown policy
  - consume the six board-profile GPIO bindings instead of duplicating pin literals
device_adapter_responsibilities:
  - require explicit STBY handling by default
  - validate resource bindings and duty range only after hardware eligibility is established
  - force zero PWM before direction changes on an eligible board
serial_log:
  - recipe=tb6612_fixed_duty_test
  - selected_devices=tb6612fng_dual_motor_driver
  - binding=PWMA:25 AIN1:27 AIN2:14 PWMB:26 BIN1:12 BIN2:13
  - hardware_blocked=TB6612_U1_STBY_pin_19_unconnected
  - no PWM or direction GPIO was configured; no motor command was issued
state_machine:
  - REPORT_BINDING
  - BLOCKED
  - COMPLETE
failure_behavior:
  - remain fail-closed and issue no peripheral writes
must_not_include:
  - a RUN_FIXED_DUTY state on the current hardware
  - a fabricated STBY GPIO
  - an external-STBY-high assumption
  - infinite motor drive loop
  - minimum-starting-duty claim
  - feedback-control claim
compile_command: idf.py -C examples_direct/tb6612_fixed_duty_test -B examples_direct/tb6612_fixed_duty_test/build_zsan_review -DMOCE_BOARD=my_board_esp32wroom build
hardware_execution_status: blocked_by_unconnected_stby
hardware_test_status: blocked_by_unconnected_stby
## END_MOCE_RECIPE_CONTEXT
