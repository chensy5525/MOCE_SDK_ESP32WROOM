# MSM261DGT003 direct PDM driver

This component contains only MSM261DGT003 device semantics and delegates I2S,
DMA, GPIO, timeout, and channel lifetime to `bsp_i2s`.

The caller must zero-initialize `Msm261dgt003DirectPdm`, call
`msm261dgt003_direct_pdm_config_default()`, and then bind the board-specific
clock and data GPIOs. The default function intentionally leaves both pins as
`GPIO_NUM_NC`; device code does not own board routing.

The driver outputs signed 16-bit mono PCM. It supports 16-48 kHz PCM rates and
selects 64x or 128x downsampling so the generated microphone clock stays in
the MSM261DGT003 standard-performance range of 1.1-4.0 MHz. L/R channel
selection must match the measured SW2/L/R voltage on the module.

No gain, filtering, volume threshold, VAD, playback, AEC, calibrated SPL, or
application state machine belongs in this driver. Calls are synchronous and
not internally serialized; one task or an application-owned mutex must own an
instance.

Minimal example: `examples_direct/msm261dgt003_direct_pdm_test`.
