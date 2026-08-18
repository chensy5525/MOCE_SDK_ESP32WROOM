## MOCE_RECIPE_CONTEXT
recipe_id: msm261dgt003_direct_pdm_test
purpose: prove bounded mono PCM acquisition from one directly connected MSM261DGT003 microphone
supported_boards:
  - my_board_esp32wroom
user_goal:
  - acquire microphone samples and observe level changes in the serial log
selected_devices:
  - msm261dgt003
transport_binding:
  transport_context: esp32_native_pdm_i2s0
  binding_ids:
    - msm261dgt003_pdm_i2s0
node_plan:
  policy: fixed
  expected_node_count: 1
  device_to_node_binding:
    - msm261dgt003 -> native I2S0
required_behavior:
  - initialize 44.1 kHz signed 16-bit mono PCM acquisition
  - periodically print sample count, peak magnitude, and mean absolute magnitude without saturating the serial log
esp32_responsibilities:
  - own the I2S0 PDM RX lifecycle
  - perform bounded reads
  - report read errors and observable PCM levels
transport_responsibilities:
  - convert PDM input to signed 16-bit PCM through I2S0 and DMA
serial_log:
  - recipe=msm261dgt003_direct_pdm_test
  - block=<number> samples=<count> peak=<magnitude> mean_abs=<magnitude>
state_machine:
  - initialize -> capture
failure_behavior:
  - invalid configuration aborts through ESP_ERROR_CHECK with an error name
  - runtime read failures are logged and the next bounded read is attempted
must_not_include:
  - calibrated SPL, VAD, recording storage, playback, or product audio policy
compile_command: .\tools\build.ps1 examples_direct/msm261dgt003_direct_pdm_test esp32 my_board_esp32wroom
hardware_test_status: integrated_passed
## END_MOCE_RECIPE_CONTEXT
