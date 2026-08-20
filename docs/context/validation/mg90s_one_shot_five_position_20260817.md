## MOCE_VALIDATION_CONTEXT
validation_id: mg90s_dual_arbitrary_angle_20260820
scope: recipe
related_contracts:
  bridge_contexts: []
  transport_contexts:
    - docs/context/transports/esp32_native_pwm_servo.md
  device_contexts:
    - docs/context/devices/mg90s_servo.md
  recipe_contexts:
    - docs/context/recipes/mg90s_one_shot_five_position_test.md
supported_boards:
  - my_board_esp32wroom
source_paths:
  components:
    - bsp/bsp_pwm
    - components_direct/servo_driver
  examples:
    - examples_direct/servo_direct_test
  docs:
    - docs/context/transports/esp32_native_pwm_servo.md
    - docs/context/devices/mg90s_servo.md
    - docs/context/recipes/mg90s_one_shot_five_position_test.md
build:
  command: .\tools\build.ps1 examples_direct/servo_direct_test esp32 my_board_esp32wroom
  verification_command: . E:\Espressif\frameworks\esp-idf-v6.0.2\export.ps1; idf.py -C D:\Desktop\Firmware\ESP\zsan\examples_direct\servo_direct_test -B D:\Desktop\Firmware\ESP\zsan\build_arbitrary_angle_20260820 build
  status: compile_passed
  toolchain: ESP-IDF 6.0.2 from E:\Espressif\frameworks\esp-idf-v6.0.2
  repository_toolchain_status: third_party/esp-idf/export.ps1 missing; repository submodule unavailable
  last_verified: 2026-08-20
  artifacts:
    - build_arbitrary_angle_20260820/servo_direct_test.bin
    - build_arbitrary_angle_20260820/servo_direct_test.elf
    - build_arbitrary_angle_20260820/servo_direct_test.map
  size:
    app_binary_bytes: 156112
    app_partition_free_percent: 85
hardware_tests:
  bench_status: untested
  board_status: untested
  integrated_status: untested
  tested_node_count: 0
  dynamic_node_assignment: not_applicable
excluded_prior_evidence:
  implementation: D:\Desktop\mg90s_driver_test
  migration_status: source_directory_not_found_after_desktop_redirect_20260820
  user_observation: effect accepted on 2026-08-17
  serial_capture: artifacts/serial/mg90s_com6_115200_after_reset_20260817.bin
  serial_capture_sha256: 098F456E00A8D991A7787B92D5CA96F31003D21B2938E1389A886A1E2C8A0296
  limitation: single-channel PWM1 evidence does not validate the current dual-channel source
expected_serial_log:
  - startup recipe, two device instances, both GPIO bindings, and PWM frequency
  - seven dual-channel commanded-angle lines for 0,17,63,91,127,180,90 degrees at approximately 3000 ms intervals
  - completion with both final commands at 90 degrees
expected_behavior:
  - both servos respond to non-45-degree target commands
  - PWM pulse width increases monotonically with the commanded integer angle
  - both servos return to the nominal 90-degree command and stop switching
failure_behavior:
  - configuration or PWM errors abort the finite sequence
  - a failure on either channel triggers the nominal 90-degree safe command for both channels
  - the recipe disables both PWM outputs if the group safe-position command fails
known_limits:
  - the dual-channel source has not been flashed or observed on hardware
  - calibration pulse targets and interpolated values are configuration results, not calibrated mechanical-angle evidence
  - serial logs prove command execution but not waveform, motion, or position accuracy
  - simultaneous two-servo supply current capability and long-duration stability are not verified
must_not_claim:
  - board-passed status for the dual-channel source
  - calibrated mechanical accuracy at any requested angle
  - position feedback or arrival confirmation
app_consumption:
  can_use_for_component_selection: true
  can_use_for_resource_planning: true
  can_use_for_hardware_build: false
  can_use_for_firmware_generation: false
approval_status: draft
## END_MOCE_VALIDATION_CONTEXT
