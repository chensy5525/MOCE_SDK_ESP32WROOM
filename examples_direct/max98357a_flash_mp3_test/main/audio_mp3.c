#include "audio_mp3.h"

#include <limits.h>

#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_types.h"

static bool s_decoder_registry_in_use;

static esp_err_t map_audio_error(esp_audio_err_t error)
{
    if (error == ESP_AUDIO_ERR_OK) {
        return ESP_OK;
    }
    if (error == ESP_AUDIO_ERR_INVALID_PARAMETER) {
        return ESP_ERR_INVALID_ARG;
    }
    if (error == ESP_AUDIO_ERR_MEM_LACK) {
        return ESP_ERR_NO_MEM;
    }
    if (error == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_FAIL;
}

esp_err_t audio_mp3_decoder_init(AudioMp3Decoder *decoder)
{
    if (decoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_decoder_registry_in_use) {
        return ESP_ERR_INVALID_STATE;
    }
    *decoder = (AudioMp3Decoder){0};

    esp_audio_err_t audio_error = esp_audio_dec_register_default();
    if (audio_error != ESP_AUDIO_ERR_OK) {
        return map_audio_error(audio_error);
    }
    audio_error = esp_audio_simple_dec_register_default();
    if (audio_error != ESP_AUDIO_ERR_OK) {
        esp_audio_dec_unregister_default();
        return map_audio_error(audio_error);
    }

    esp_audio_simple_dec_cfg_t config = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,
        .cfg_size = 0,
        .use_frame_dec = false,
    };
    esp_audio_simple_dec_handle_t handle = NULL;
    audio_error = esp_audio_simple_dec_open(&config, &handle);
    if (audio_error != ESP_AUDIO_ERR_OK) {
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return map_audio_error(audio_error);
    }

    decoder->decoder_handle = handle;
    decoder->initialized = true;
    s_decoder_registry_in_use = true;
    return ESP_OK;
}

esp_err_t audio_mp3_decoder_process(AudioMp3Decoder *decoder,
                                    uint8_t *input,
                                    size_t input_size,
                                    bool end_of_stream,
                                    int16_t *pcm_output,
                                    size_t pcm_output_capacity_bytes,
                                    size_t *input_consumed,
                                    size_t *pcm_output_size,
                                    size_t *pcm_output_needed)
{
    if ((decoder == NULL) || !decoder->initialized ||
        (decoder->decoder_handle == NULL) || (pcm_output == NULL) ||
        (input_consumed == NULL) || (pcm_output_size == NULL) ||
        (pcm_output_needed == NULL) ||
        ((input == NULL) && (input_size > 0U)) ||
        (input_size > UINT32_MAX) ||
        (pcm_output_capacity_bytes == 0U) ||
        (pcm_output_capacity_bytes > UINT32_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    *input_consumed = 0U;
    *pcm_output_size = 0U;
    *pcm_output_needed = 0U;
    esp_audio_simple_dec_raw_t raw = {
        .buffer = input,
        .len = (uint32_t)input_size,
        .eos = end_of_stream,
    };
    esp_audio_simple_dec_out_t output = {
        .buffer = (uint8_t *)pcm_output,
        .len = (uint32_t)pcm_output_capacity_bytes,
    };

    esp_audio_err_t audio_error = esp_audio_simple_dec_process(
        (esp_audio_simple_dec_handle_t)decoder->decoder_handle, &raw, &output);
    *input_consumed = raw.consumed;
    *pcm_output_size = output.decoded_size;
    *pcm_output_needed = output.needed_size;
    return map_audio_error(audio_error);
}

esp_err_t audio_mp3_decoder_get_info(AudioMp3Decoder *decoder,
                                     AudioMp3Info *info)
{
    if ((decoder == NULL) || (info == NULL) || !decoder->initialized ||
        (decoder->decoder_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_audio_simple_dec_info_t decoder_info = {0};
    esp_audio_err_t audio_error = esp_audio_simple_dec_get_info(
        (esp_audio_simple_dec_handle_t)decoder->decoder_handle, &decoder_info);
    if (audio_error != ESP_AUDIO_ERR_OK) {
        return map_audio_error(audio_error);
    }

    *info = (AudioMp3Info){
        .sample_rate_hz = decoder_info.sample_rate,
        .bitrate_bps = decoder_info.bitrate,
        .encoded_frame_size = decoder_info.frame_size,
        .bits_per_sample = decoder_info.bits_per_sample,
        .channel_count = decoder_info.channel,
        .valid = true,
    };
    return ESP_OK;
}

esp_err_t audio_mp3_decoder_deinit(AudioMp3Decoder *decoder)
{
    if ((decoder == NULL) || !decoder->initialized ||
        (decoder->decoder_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_audio_simple_dec_close(
        (esp_audio_simple_dec_handle_t)decoder->decoder_handle);
    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
    decoder->decoder_handle = NULL;
    decoder->initialized = false;
    s_decoder_registry_in_use = false;
    return ESP_OK;
}
