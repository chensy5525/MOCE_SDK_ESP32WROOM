#include "audio_core.h"

#include <limits.h>

#define AUDIO_GAIN_Q16_ONE 65536

static int16_t saturate_i16(int64_t value, uint64_t *limited_sample_count)
{
    if (value > INT16_MAX) {
        (*limited_sample_count)++;
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        (*limited_sample_count)++;
        return INT16_MIN;
    }
    return (int16_t)value;
}

static AudioStereoFrame read_input_frame(const int16_t *input,
                                         size_t index,
                                         uint8_t channel_count)
{
    AudioStereoFrame frame = {0};
    if (channel_count == 1U) {
        frame.left = input[index];
        frame.right = input[index];
    } else {
        /* Downmix before resampling so a single speaker receives both sides. */
        const int32_t left = input[index * 2U];
        const int32_t right = input[(index * 2U) + 1U];
        const int16_t mono = (int16_t)((left + right) / 2);
        frame.left = mono;
        frame.right = mono;
    }
    return frame;
}

esp_err_t audio_resampler_init(AudioResampler *resampler,
                               uint32_t input_rate_hz,
                               uint32_t output_rate_hz)
{
    if ((resampler == NULL) || (input_rate_hz == 0U) ||
        (output_rate_hz == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }
    *resampler = (AudioResampler){
        .input_rate_hz = input_rate_hz,
        .output_rate_hz = output_rate_hz,
    };
    return ESP_OK;
}

esp_err_t audio_resampler_process(AudioResampler *resampler,
                                  const int16_t *input,
                                  size_t input_frame_count,
                                  uint8_t input_channel_count,
                                  AudioStereoFrame *output,
                                  size_t output_capacity,
                                  size_t *input_frames_consumed,
                                  size_t *output_frames_produced)
{
    if ((resampler == NULL) || (input == NULL) || (output == NULL) ||
        (input_frames_consumed == NULL) || (output_frames_produced == NULL) ||
        (input_frame_count == 0U) || (output_capacity == 0U) ||
        ((input_channel_count != 1U) && (input_channel_count != 2U)) ||
        (resampler->input_rate_hz == 0U) ||
        (resampler->output_rate_hz == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    *input_frames_consumed = 0U;
    *output_frames_produced = 0U;
    while (*input_frames_consumed < input_frame_count) {
        AudioStereoFrame current = read_input_frame(
            input, *input_frames_consumed, input_channel_count);
        if (!resampler->has_previous) {
            resampler->previous = current;
            resampler->has_previous = true;
            (*input_frames_consumed)++;
            continue;
        }

        uint64_t probe_tick = resampler->next_tick;
        size_t required = 0U;
        while (probe_tick < resampler->output_rate_hz) {
            required++;
            probe_tick += resampler->input_rate_hz;
        }
        if (required > (output_capacity - *output_frames_produced)) {
            break;
        }

        while (resampler->next_tick < resampler->output_rate_hz) {
            const int64_t left_delta =
                (int64_t)current.left - resampler->previous.left;
            const int64_t right_delta =
                (int64_t)current.right - resampler->previous.right;
            AudioStereoFrame *destination = &output[*output_frames_produced];
            destination->left = (int16_t)(resampler->previous.left +
                ((left_delta * (int64_t)resampler->next_tick) /
                 (int64_t)resampler->output_rate_hz));
            destination->right = (int16_t)(resampler->previous.right +
                ((right_delta * (int64_t)resampler->next_tick) /
                 (int64_t)resampler->output_rate_hz));
            (*output_frames_produced)++;
            resampler->next_tick += resampler->input_rate_hz;
        }
        resampler->next_tick -= resampler->output_rate_hz;
        resampler->previous = current;
        (*input_frames_consumed)++;
    }
    return (*input_frames_consumed == 0U) ? ESP_ERR_NO_MEM : ESP_OK;
}

esp_err_t audio_resampler_flush(AudioResampler *resampler,
                                AudioStereoFrame *output,
                                size_t output_capacity,
                                size_t *output_frames_produced)
{
    if ((resampler == NULL) || (output == NULL) ||
        (output_frames_produced == NULL) || (output_capacity == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }
    *output_frames_produced = 0U;
    if (resampler->has_previous && (resampler->next_tick == 0U)) {
        output[0] = resampler->previous;
        *output_frames_produced = 1U;
    }
    resampler->has_previous = false;
    resampler->next_tick = 0U;
    return ESP_OK;
}

esp_err_t audio_mixer_init(AudioMixer *mixer, uint16_t initial_volume_permille)
{
    if ((mixer == NULL) ||
        (initial_volume_permille > AUDIO_CORE_UNITY_GAIN_PERMILLE)) {
        return ESP_ERR_INVALID_ARG;
    }
    const int32_t gain_q16 =
        ((int32_t)initial_volume_permille * AUDIO_GAIN_Q16_ONE) /
        AUDIO_CORE_UNITY_GAIN_PERMILLE;
    *mixer = (AudioMixer){
        .current_gain_q16 = gain_q16,
        .target_gain_q16 = gain_q16,
    };
    return ESP_OK;
}

esp_err_t audio_mixer_set_volume(AudioMixer *mixer,
                                 uint16_t volume_permille,
                                 uint32_t ramp_frames)
{
    if ((mixer == NULL) ||
        (volume_permille > AUDIO_CORE_UNITY_GAIN_PERMILLE)) {
        return ESP_ERR_INVALID_ARG;
    }
    mixer->target_gain_q16 =
        ((int32_t)volume_permille * AUDIO_GAIN_Q16_ONE) /
        AUDIO_CORE_UNITY_GAIN_PERMILLE;
    mixer->ramp_start_gain_q16 = mixer->current_gain_q16;
    mixer->ramp_frames_total = ramp_frames;
    mixer->ramp_frames_remaining = ramp_frames;
    if (ramp_frames == 0U) {
        mixer->current_gain_q16 = mixer->target_gain_q16;
    }
    return ESP_OK;
}

esp_err_t audio_mixer_mix(AudioMixer *mixer,
                          const AudioStereoFrame *const sources[],
                          const uint16_t source_gains_permille[],
                          size_t source_count,
                          size_t frame_count,
                          int16_t *output_interleaved)
{
    if ((mixer == NULL) || (sources == NULL) ||
        (source_gains_permille == NULL) || (output_interleaved == NULL) ||
        (source_count == 0U) || (source_count > AUDIO_CORE_MAX_SOURCES) ||
        (frame_count == 0U) || (frame_count > (SIZE_MAX / 2U))) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t source = 0U; source < source_count; source++) {
        if ((sources[source] == NULL) ||
            (source_gains_permille[source] >
             AUDIO_CORE_MAX_SOURCE_GAIN_PERMILLE)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    for (size_t frame = 0U; frame < frame_count; frame++) {
        int64_t mixed_left_q16 = 0;
        int64_t mixed_right_q16 = 0;
        for (size_t source = 0U; source < source_count; source++) {
            const int32_t source_gain_q16 =
                ((int32_t)source_gains_permille[source] * AUDIO_GAIN_Q16_ONE) /
                AUDIO_CORE_UNITY_GAIN_PERMILLE;
            mixed_left_q16 +=
                (int64_t)sources[source][frame].left * source_gain_q16;
            mixed_right_q16 +=
                (int64_t)sources[source][frame].right * source_gain_q16;
        }

        const int64_t divisor =
            (int64_t)AUDIO_GAIN_Q16_ONE * AUDIO_GAIN_Q16_ONE;
        int64_t left =
            (mixed_left_q16 * mixer->current_gain_q16) / divisor;
        int64_t right =
            (mixed_right_q16 * mixer->current_gain_q16) / divisor;
        output_interleaved[frame * 2U] =
            saturate_i16(left, &mixer->limited_sample_count);
        output_interleaved[(frame * 2U) + 1U] =
            saturate_i16(right, &mixer->limited_sample_count);

        if (mixer->ramp_frames_remaining > 0U) {
            mixer->ramp_frames_remaining--;
            const uint32_t elapsed = mixer->ramp_frames_total -
                                     mixer->ramp_frames_remaining;
            const int64_t delta = (int64_t)mixer->target_gain_q16 -
                                  mixer->ramp_start_gain_q16;
            mixer->current_gain_q16 = mixer->ramp_start_gain_q16 +
                (int32_t)((delta * elapsed) / mixer->ramp_frames_total);
        }
    }
    return ESP_OK;
}

uint64_t audio_mixer_get_limited_sample_count(const AudioMixer *mixer)
{
    return (mixer == NULL) ? 0U : mixer->limited_sample_count;
}
