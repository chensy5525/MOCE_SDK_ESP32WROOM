## MOCE_VALIDATION_CONTEXT
validation_id: mg90s_one_shot_five_position_20260817
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
  status: compile_and_link_passed_with_local_toolchain
  toolchain: ESP-IDF 6.0.2 from E:\Espressif\frameworks\esp-idf-v6.0.2
  repository_toolchain_status: third_party/esp-idf/export.ps1 missing; repository submodule unavailable
  last_verified: 2026-08-17
  artifacts:
    - examples_direct/servo_direct_test/build/servo_direct_test.bin
    - examples_direct/servo_direct_test/build/servo_direct_test.elf
    - examples_direct/servo_direct_test/build/servo_direct_test.map
  size:
    app_binary_bytes: 154816
    app_partition_free_percent: 85
    flash_code_bytes: 59350
    flash_data_bytes: 38920
    iram_bytes: 45451
    dram_bytes: 13556
hardware_tests:
  bench_status: partial
  board_status: untested
  integrated_status: untested
  tested_node_count: 0
  dynamic_node_assignment: not_applicable
prior_baseline_evidence:
  implementation: C:\Users\LENOVO\Desktop\mg90s_driver_test
  user_observation: effect accepted on 2026-08-17
  serial_capture: artifacts/serial/mg90s_com6_115200_after_reset_20260817.bin
  serial_capture_sha256: 098F456E00A8D991A7787B92D5CA96F31003D21B2938E1389A886A1E2C8A0296
  limitation: baseline evidence does not validate the refactored BSP-based source
expected_serial_log:
  - startup recipe, selected device, GPIO, and PWM frequency
  - seven commanded position lines at approximately 3000 ms intervals
  - completion at the final 90-degree command
expected_behavior:
  - the servo produces five distinguishable nominal positions
  - every adjacent command changes by one 45-degree step
  - the servo returns to the nominal 90-degree command and stops switching
failure_behavior:
  - configuration or PWM errors abort the finite sequence
  - the recipe requests the nominal 90-degree safe command after a runtime error
  - the recipe disables PWM if the safe-position command also fails
known_limits:
  - the refactored source has not been flashed or observed on hardware
  - pulse targets are configuration values, not calibrated angle evidence
  - serial logs prove command execution but not waveform, motion, or position accuracy
  - servo supply current capability and long-duration stability are not verified
must_not_claim:
  - board-passed status for the refactored source
  - calibrated 0, 45, 90, 135, or 180 degree accuracy
  - arbitrary angle positioning or position feedback
app_consumption:
  can_use_for_component_selection: true
  can_use_for_resource_planning: true
  can_use_for_hardware_build: true
  can_use_for_firmware_generation: false
approval_status: draft
## END_MOCE_VALIDATION_CONTEXT
