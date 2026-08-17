## MOCE_TRANSPORT_CONTEXT
transport_id: esp32_native_pdm_i2s0
purpose: acquire mono PCM from a PDM input through ESP32 I2S0 and DMA
supported_host_boards:
  - my_board_esp32wroom
implementation:
  component: bsp/bsp_i2s
  public_operations:
    - bsp_i2s_pdm_rx_init
    - bsp_i2s_pdm_rx_start
    - bsp_i2s_pdm_rx_read
    - bsp_i2s_pdm_rx_stop
    - bsp_i2s_pdm_rx_deinit
generic_operations:
  - op: pdm_rx_init
    fields: clk_gpio, data_gpio, sample_rate_hz, downsample, channel, invert_clk
  - op: pdm_rx_start
  - op: pdm_rx_read
    fields: sample_buffer, sample_capacity, timeout_ms
  - op: pdm_rx_stop
  - op: pdm_rx_deinit
resource_bindings:
  - binding_id: msm261dgt003_pdm_i2s0
    peripheral: I2S0
    clk_gpio: 18
    data_gpio: 2
    share_rules: exclusive I2S channel and GPIO ownership
electrical_constraints:
  logic_voltage_v: 3.3
  common_ground_required: true
resource_conflicts:
  - GPIO18 is also listed as the board SPI SCK resource and cannot be used by SPI while PDM is active
  - GPIO2 is a boot-strapping pin and requires repeated cold-boot and download-mode validation
transport_responsibilities:
  - configure I2S PDM RX and DMA
  - perform bounded synchronous reads into caller-owned buffers
  - stop and release the I2S channel
must_not_include:
  - microphone-specific gain or volume policy
  - product audio state machines
validation_status: compile_passed
## END_MOCE_TRANSPORT_CONTEXT
