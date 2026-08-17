# Native PDM transport

This component owns one ESP32 I2S receive channel and exposes bounded,
synchronous PDM-to-PCM operations. It contains no microphone-specific clock
selection, gain, volume policy, or product behavior.

The caller must zero-initialize each `NativePdmTransport` instance and owns
configuration and serialization. ESP32 PDM RX uses I2S0; the
clock GPIO, data GPIO, I2S0 channel, and DMA resources remain exclusive until
`native_pdm_transport_deinit()` succeeds. `native_pdm_transport_read()` writes
only into the caller-provided buffer and rejects waits longer than 60 seconds.
