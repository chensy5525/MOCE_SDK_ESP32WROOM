#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CORE_MAX_SOURCES               4U
#define AUDIO_CORE_UNITY_GAIN_PERMILLE       1000U
#define AUDIO_CORE_MAX_SOURCE_GAIN_PERMILLE  2000U

typedef struct {
    int16_t left;
    int16_t right;
} AudioStereoFrame;

typedef struct {
    uint32_t input_rate_hz;
    uint32_t output_rate_hz;
    uint64_t next_tick;
    AudioStereoFrame previous;
    bool has_previous;
} AudioResampler;

typedef struct {
    int32_t current_gain_q16;
    int32_t target_gain_q16;
    int32_t ramp_start_gain_q16;
    uint32_t ramp_frames_total;
    uint32_t ramp_frames_remaining;
    uint64_t limited_sample_count;
} AudioMixer;

esp_err_t audio_resampler_init(AudioResampler *resampler,
                               uint32_t input_rate_hz,
                               uint32_t output_rate_hz);
esp_err_t audio_resampler_process(AudioResampler *resampler,
                                  const int16_t *input,
                                  size_t input_frame_count,
                                  uint8_t input_channel_count,
                                  AudioStereoFrame *output,
                                  size_t output_capacity,
                                  size_t *input_frames_consumed,
                                  size_t *output_frames_produced);
esp_err_t audio_resampler_flush(AudioResampler *resampler,
                                AudioStereoFrame *output,
                                size_t output_capacity,
                                size_t *output_frames_produced);

esp_err_t audio_mixer_init(AudioMixer *mixer, uint16_t initial_volume_permille);
esp_err_t audio_mixer_set_volume(AudioMixer *mixer,
                                 uint16_t volume_permille,
                                 uint32_t ramp_frames);
esp_err_t audio_mixer_mix(AudioMixer *mixer,
                          const AudioStereoFrame *const sources[],
                          const uint16_t source_gains_permille[],
                          size_t source_count,
                          size_t frame_count,
                          int16_t *output_interleaved);
uint64_t audio_mixer_get_limited_sample_count(const AudioMixer *mixer);

#ifdef __cplusplus
}
#endif
