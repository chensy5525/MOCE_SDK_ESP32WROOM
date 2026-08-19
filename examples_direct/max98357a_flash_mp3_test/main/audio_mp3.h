#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t bitrate_bps;
    uint32_t encoded_frame_size;
    uint8_t bits_per_sample;
    uint8_t channel_count;
    bool valid;
} AudioMp3Info;

typedef struct {
    void *decoder_handle;
    bool initialized;
} AudioMp3Decoder;

esp_err_t audio_mp3_decoder_init(AudioMp3Decoder *decoder);
esp_err_t audio_mp3_decoder_process(AudioMp3Decoder *decoder,
                                    uint8_t *input,
                                    size_t input_size,
                                    bool end_of_stream,
                                    int16_t *pcm_output,
                                    size_t pcm_output_capacity_bytes,
                                    size_t *input_consumed,
                                    size_t *pcm_output_size,
                                    size_t *pcm_output_needed);
esp_err_t audio_mp3_decoder_get_info(AudioMp3Decoder *decoder,
                                     AudioMp3Info *info);
esp_err_t audio_mp3_decoder_deinit(AudioMp3Decoder *decoder);

#ifdef __cplusplus
}
#endif
