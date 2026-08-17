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
    - components_direct/native_pdm_transport
    - components_direct/msm261dgt003_direct_pdm
  examples: []
  docs:
    - docs/context/devices/msm261dgt003.md
    - docs/context/transports/esp32_native_pdm_i2s0.md
build:
  command: CMake configure followed by Ninja component-object and static-library targets in an ignored temporary project
  status: compile_passed
  scope: native_pdm_transport, msm261dgt003_direct_pdm, and public-API caller objects plus static libraries
  full_application_link: untested
  compiler_flags: ESP-IDF defaults including -Wall and -Werror
  toolchain: ESP-IDF 6.0.2, xtensa-esp-elf GCC 15.2.0; repository submodule unavailable
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
  - no minimal example is included in this driver-only task
  - only component objects and static libraries were built; no application image was linked
  - the L/R switch level must be measured and matched by configuration
  - GPIO2 boot behavior and GPIO18 SPI conflict are not board-validated here
  - no compile, flash, waveform, audio-quality, or long-duration claim is inherited from the standalone prototype
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
