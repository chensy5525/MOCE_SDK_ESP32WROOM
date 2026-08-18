## MOCE_VALIDATION_CONTEXT
validation_id: msm261dgt003_direct_pdm_20260817
scope: device
related_contracts:
  bridge_contexts: []
  transport_contexts:
    - docs/context/transports/esp32_native_pdm_i2s0.md
  device_contexts:
    - docs/context/devices/msm261dgt003.md
  recipe_contexts:
    - docs/context/recipes/msm261dgt003_direct_pdm_test.md
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
  command: idf.py --no-ccache -C examples_direct/msm261dgt003_direct_pdm_test build
  status: compile_passed
  scope: full example configure, compile, link, binary generation, and size check after BSP split
  full_application_link: passed
  compiler_flags: ESP-IDF defaults including -Wall and -Werror
  toolchain: ESP-IDF 6.0.2, xtensa-esp-elf GCC 15.2.0; repository submodule unavailable
  image_size_bytes: 161488
  app_partition_size_bytes: 1048576
  app_partition_free_percent: 85
  iram_used_bytes: 46171
  iram_used_percent: 35.23
  dram_used_bytes: 14472
  dram_used_percent: 8.01
  last_verified: 2026-08-18
hardware_tests:
  bench_status: partial
  board_status: passed
  integrated_status: partial
  tested_node_count: 1
  dynamic_node_assignment: not_applicable
  target: ESP32-D0WD-V3 revision 3.1 on CH340 COM6
  flash_result: bootloader, partition table, and app hashes verified by esptool 5.3.1
  reset_result: POWERON_RESET followed by normal SPI_FAST_FLASH_BOOT
  observed_initialization:
    - I2S0 PDM RX initialized on CLK GPIO18 and DATA GPIO2
    - PCM 44100 Hz, PDM clock 2822400 Hz, DSR 64
  observed_runtime:
    - continuous 512-sample blocks with no read error or reset during bounded captures
    - one log every 32 blocks arrived about every 370 ms, matching the configured sample rate
    - ambient peak was approximately 1083-1309 and mean_abs approximately 1016-1203
  acoustic_stimulus_result: inconclusive; a two-second host WAV stimulus did not produce a sustained level increase
  evidence:
    - raw_log: build/log/mic_bench_20260818.bin
      bytes: 2400
      sha256: 4FFC979CEFF003883B8D137BC7A30BFFE8B936A84DF797581014E88F4895980E
    - raw_log: build/log/mic_wav_20260818.bin
      bytes: 1656
      sha256: 464E711D24AA452E174C73DF06DC9247530D3F767EA1A068C2C4321E560D9941
prior_baseline_evidence:
  implementation: pre-rename standalone MSM261DGT003 PDM implementation
  date: 2026-08-15
  result: 44.1 kHz PCM acquisition and voice-responsive RMS were observed
  limitation: prior evidence does not validate the renamed repository components
expected_serial_log:
  - recipe=msm261dgt003_direct_pdm_test
  - transport=I2S0 PDM RX, CLK=GPIO18, DATA=GPIO2
  - PCM=44100 Hz, PDM CLK=2822400 Hz, DSR=64
  - periodically: block=<n> samples=512 peak=<n> mean_abs=<n>
expected_behavior:
  - a caller-provided PCM buffer receives signed 16-bit mono samples
failure_behavior:
  - invalid pins, rates, channels, buffers, or timeouts return an error
  - transport initialization failure attempts to release the allocated I2S channel before returning the initialization error
known_limits:
  - the physical L/R switch level was not measured; current firmware selects the low/left slot
  - GPIO2 cold boot passed on the tested board, but GPIO18 remains exclusive with SPI SCK while I2S is active
  - acoustic sensitivity, audio quality, waveform fidelity, and long-duration stability remain unverified
must_not_claim:
  - acoustically verified microphone response or full integrated pass
  - calibrated SPL, VAD, playback, AEC, or production audio readiness
app_consumption:
  can_use_for_component_selection: true
  can_use_for_resource_planning: true
  can_use_for_hardware_build: false
  can_use_for_firmware_generation: false
approval_status: blocked
## END_MOCE_VALIDATION_CONTEXT
