# bsp_i2s

Minimal board-support component for ESP32 I2S0 PDM receive mode.

It owns the single I2S0 RX channel, PDM-to-PCM conversion, bounded synchronous
reads, and channel cleanup. It does not contain microphone model limits,
product audio policy, filtering, gain, VAD, or playback behavior.

The component is not internally serialized. One task, or an application-owned
mutex, must own the complete init/start/read/stop/deinit lifecycle.

Current board binding:

- PDM clock: GPIO18 (`I2S_SCK`)
- PDM data: GPIO2 (`I2S_SD`)
- Peripheral: I2S0, exclusive while active

GPIO18 conflicts with the SPI clock net. GPIO2 is a strapping pin, so cold boot
and download-mode behavior must be validated on hardware.
