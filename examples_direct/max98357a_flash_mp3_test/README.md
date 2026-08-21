# MAX98357A Flash MP3 test

This example reads `/audio/test.mp3` from an embedded FATFS image, decodes it
with Espressif `esp_audio_codec`, downmixes stereo PCM to mono and duplicates
that mono signal into both I2S slots, converts it to 48 kHz, applies volume
and limiting, and streams it to MAX98357A over I2S0. The downmix prevents a
single speaker from losing content that exists on only one source channel.

MP3 decoding, resampling, volume ramping, mixing, and playback policy are kept
inside this example. The reusable `max98357a_direct` component only initializes
the I2S output path and writes raw interleaved stereo PCM frames.

## Wiring

| Signal | ESP32 GPIO | Adapter signal |
|---|---:|---|
| BCLK | 18 | I2S_SCK |
| WS/LRCLK | 19 | I2S_WS |
| DATA | 2 | I2S_SD |
| Power | 5 V | 5V |
| Ground | GND | GND |

The speaker and PDM microphone share GPIO18, GPIO2, and I2S0. They cannot run
at the same time on this board profile.

Before first playback, set the adapter gain switch to the lower-gain position
and verify that the speaker is connected across OUTP/OUTN, never from either
output to ground.

## Build

Copy the MP3 as `audio_data/test.mp3`, then run from an ESP-IDF 6.0.2 shell:

```powershell
idf.py -B build set-target esp32
idf.py -B build build
```

Flashing is intentionally a separate, confirmation-gated operation.

See [AUDIO_IMPLEMENTATION.md](AUDIO_IMPLEMENTATION.md) for the processing
pipeline, implementation differences, validation evidence, and remaining
integration limits.
