## MOCE_VALIDATION_CONTEXT
validation_id: msm261dgt003_direct_pdm_20260817
scope: device
related_contracts:
  bridge_contexts: []
  transport_contexts:
    - docs/context/transports/esp32_native_pdm_i2s0.md
  device_contexts:
    - docs/context/devices/msm261dgt003.md
  recipe_contexts: []
supported_boards:
  - my_board_esp32wroom
source_paths:
  components:
    - bsp/bsp_i2s
    - components_direct/msm261dgt003_direct_pdm
  examples:
    - examples_direct/msm261dgt003_direct_pdm_test
  docs:
    - docs/context/devices/msm261dgt003.md
    - docs/context/transports/esp32_native_pdm_i2s0.md
    - docs/context/recipes/msm261dgt003_direct_pdm_test.md
build:
  command: idf.py -C examples_direct/msm261dgt003_direct_pdm_test build
  status: compile_passed
  scope: full example configure, compile, link, binary generation, and size check after BSP split
  full_application_link: passed
  compiler_flags: ESP-IDF defaults including -Wall and -Werror
  toolchain: ESP-IDF 6.0.2, xtensa-esp-elf GCC 15.2.0; repository submodule unavailable
  image_size_bytes: 161472
  app_partition_size_bytes: 1048576
  app_partition_free_percent: 85
  iram_used_bytes: 46171
  iram_used_percent: 35.23
  dram_used_bytes: 14472
  dram_used_percent: 8.01
  last_verified: 2026-08-17
hardware_tests:
  bench_status: untested
  board_status: untested
  integrated_status: untested
  tested_node_count: 0
  dynamic_node_assignment: not_applicable
prior_baseline_evidence:
  implementation: pre-rename standalone MSM261DGT003 PDM implementation
  date: 2026-08-15
  result: 44.1 kHz PCM acquisition and voice-responsive RMS were observed
  limitation: prior evidence does not validate the renamed repository components
expected_serial_log:
  - PCM=44100 Hz, PDM CLK=2822400 Hz, DSR=64
expected_behavior:
  - a caller-provided PCM buffer receives signed 16-bit mono samples
failure_behavior:
  - invalid pins, rates, channels, buffers, or timeouts return an error
  - transport initialization failure attempts to release the allocated I2S channel before returning the initialization error
known_limits:
  - the minimal example has not been flashed or run on hardware
  - the L/R switch level must be measured and matched by configuration
  - GPIO2 boot behavior and GPIO18 SPI conflict are not board-validated here
  - no flash, waveform, audio-quality, or long-duration claim is inherited from the standalone prototype
must_not_claim:
  - board-passed status for these repository files
  - calibrated SPL, VAD, playback, AEC, or production audio readiness
app_consumption:
  can_use_for_component_selection: true
  can_use_for_resource_planning: true
  can_use_for_hardware_build: false
  can_use_for_firmware_generation: false
approval_status: blocked
## END_MOCE_VALIDATION_CONTEXT
