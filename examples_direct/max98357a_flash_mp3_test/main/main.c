#include <stdio.h>
#include <string.h>

#include "audio_core.h"
#include "audio_mp3.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "max98357a_direct.h"
#include "wear_levelling.h"

#define AUDIO_MOUNT_PATH            "/audio"
#define AUDIO_PARTITION_LABEL       "audio"
#define AUDIO_FILE_PATH             AUDIO_MOUNT_PATH "/test.mp3"
#define MP3_INPUT_BUFFER_BYTES      2048U
#define MP3_PCM_BUFFER_BYTES        8192U
#define RESAMPLED_BUFFER_FRAMES     512U
#define PLAYBACK_SAMPLE_RATE_HZ     48000U
#define PLAYBACK_VOLUME_PERMILLE    1000U
#define PLAYBACK_VOLUME_RAMP_FRAMES 4800U
#define PROGRESS_LOG_FRAMES         (PLAYBACK_SAMPLE_RATE_HZ * 5U)

static const char *TAG = "max98357a_mp3";

static uint8_t s_mp3_input[MP3_INPUT_BUFFER_BYTES];
static int16_t s_mp3_pcm[MP3_PCM_BUFFER_BYTES / sizeof(int16_t)];
static AudioStereoFrame s_resampled[RESAMPLED_BUFFER_FRAMES];
static int16_t s_playback[RESAMPLED_BUFFER_FRAMES * 2U];
static AudioMp3Decoder s_decoder;
static AudioResampler s_resampler;
static AudioMixer s_mixer;
static Max98357aDirect s_amplifier;

_Static_assert(sizeof(AudioStereoFrame) == (2U * sizeof(int16_t)),
               "AudioStereoFrame must be packed stereo PCM");

static esp_err_t write_resampled_frames(const AudioStereoFrame *frames,
                                        size_t frame_count)
{
    if ((frames == NULL) || (frame_count == 0U) ||
        (frame_count > RESAMPLED_BUFFER_FRAMES)) {
        return ESP_ERR_INVALID_ARG;
    }

    const AudioStereoFrame *sources[1] = {frames};
    const uint16_t gains[1] = {AUDIO_CORE_UNITY_GAIN_PERMILLE};
    esp_err_t error = audio_mixer_mix(&s_mixer,
                                      sources,
                                      gains,
                                      1U,
                                      frame_count,
                                      s_playback);
    if (error != ESP_OK) {
        return error;
    }
    return max98357a_direct_write_pcm(&s_amplifier,
                                      s_playback,
                                      frame_count);
}

static esp_err_t play_decoded_pcm(const int16_t *pcm,
                                  size_t pcm_bytes,
                                  const AudioMp3Info *info,
                                  uint64_t *played_frames)
{
    if ((pcm == NULL) || (info == NULL) || (played_frames == NULL) ||
        !info->valid || (info->bits_per_sample != 16U) ||
        ((info->channel_count != 1U) && (info->channel_count != 2U))) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t bytes_per_frame =
        (size_t)info->channel_count * sizeof(int16_t);
    if ((pcm_bytes == 0U) || ((pcm_bytes % bytes_per_frame) != 0U)) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t input_frames_remaining = pcm_bytes / bytes_per_frame;
    const int16_t *input_cursor = pcm;
    while (input_frames_remaining > 0U) {
        size_t consumed = 0U;
        size_t produced = 0U;
        esp_err_t error = audio_resampler_process(&s_resampler,
                                                  input_cursor,
                                                  input_frames_remaining,
                                                  info->channel_count,
                                                  s_resampled,
                                                  RESAMPLED_BUFFER_FRAMES,
                                                  &consumed,
                                                  &produced);
        if ((error != ESP_OK) && (error != ESP_ERR_NO_MEM)) {
            return error;
        }
        if (produced > 0U) {
            error = write_resampled_frames(s_resampled, produced);
            if (error != ESP_OK) {
                return error;
            }
            *played_frames += produced;
        }
        if (consumed == 0U) {
            return ESP_ERR_INVALID_SIZE;
        }
        input_cursor += consumed * info->channel_count;
        input_frames_remaining -= consumed;
    }
    return ESP_OK;
}

static esp_err_t decode_and_play(FILE *file, long file_size)
{
    AudioMp3Info info = {0};
    uint64_t played_frames = 0U;
    uint64_t next_progress_frame = PROGRESS_LOG_FRAMES;
    long file_position = 0L;

    while (file_position < file_size) {
        size_t requested = MP3_INPUT_BUFFER_BYTES;
        const long remaining = file_size - file_position;
        if (remaining < (long)requested) {
            requested = (size_t)remaining;
        }
        const size_t bytes_read = fread(s_mp3_input, 1U, requested, file);
        if (bytes_read != requested) {
            return ferror(file) ? ESP_FAIL : ESP_ERR_INVALID_SIZE;
        }
        file_position += (long)bytes_read;
        const bool end_of_stream = file_position == file_size;

        size_t offset = 0U;
        uint32_t no_progress_count = 0U;
        while (offset < bytes_read) {
            size_t consumed = 0U;
            size_t pcm_size = 0U;
            size_t pcm_needed = 0U;
            esp_err_t error = audio_mp3_decoder_process(
                &s_decoder,
                &s_mp3_input[offset],
                bytes_read - offset,
                end_of_stream,
                s_mp3_pcm,
                sizeof(s_mp3_pcm),
                &consumed,
                &pcm_size,
                &pcm_needed);
            if (error == ESP_ERR_INVALID_SIZE) {
                ESP_LOGE(TAG, "PCM buffer too small: have %u, need %u bytes",
                         (unsigned)sizeof(s_mp3_pcm), (unsigned)pcm_needed);
                return error;
            }
            if (error != ESP_OK) {
                return error;
            }
            offset += consumed;

            if (pcm_size > 0U) {
                if (!info.valid) {
                    error = audio_mp3_decoder_get_info(&s_decoder, &info);
                    if (error != ESP_OK) {
                        return error;
                    }
                    if ((info.bits_per_sample != 16U) ||
                        ((info.channel_count != 1U) &&
                         (info.channel_count != 2U))) {
                        ESP_LOGE(TAG, "unsupported PCM: %u-bit, %u channel(s)",
                                 info.bits_per_sample, info.channel_count);
                        return ESP_ERR_NOT_SUPPORTED;
                    }
                    error = audio_resampler_init(&s_resampler,
                                                 info.sample_rate_hz,
                                                 PLAYBACK_SAMPLE_RATE_HZ);
                    if (error != ESP_OK) {
                        return error;
                    }
                    ESP_LOGI(TAG,
                             "MP3: %lu Hz, %u-bit, %u channel(s), %lu bps",
                             (unsigned long)info.sample_rate_hz,
                             info.bits_per_sample,
                             info.channel_count,
                             (unsigned long)info.bitrate_bps);
                }
                error = play_decoded_pcm(s_mp3_pcm,
                                         pcm_size,
                                         &info,
                                         &played_frames);
                if (error != ESP_OK) {
                    return error;
                }
            }

            if ((consumed == 0U) && (pcm_size == 0U)) {
                no_progress_count++;
                if (no_progress_count >= 2U) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
            } else {
                no_progress_count = 0U;
            }
            if (played_frames >= next_progress_frame) {
                ESP_LOGI(TAG, "played %llu seconds",
                         played_frames / PLAYBACK_SAMPLE_RATE_HZ);
                next_progress_frame += PROGRESS_LOG_FRAMES;
            }
        }
    }

    if (!info.valid) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    size_t produced = 0U;
    esp_err_t error = audio_resampler_flush(&s_resampler,
                                            s_resampled,
                                            RESAMPLED_BUFFER_FRAMES,
                                            &produced);
    if ((error == ESP_OK) && (produced > 0U)) {
        error = write_resampled_frames(s_resampled, produced);
        played_frames += produced;
    }
    if (error == ESP_OK) {
        ESP_LOGI(TAG, "playback complete: %llu frames, limiter events: %llu",
                 played_frames,
                 audio_mixer_get_limited_sample_count(&s_mixer));
    }
    return error;
}

void app_main(void)
{
    wl_handle_t wl_handle = WL_INVALID_HANDLE;
    FILE *audio_file = NULL;
    bool decoder_initialized = false;
    bool amplifier_initialized = false;
    bool filesystem_mounted = false;
    esp_err_t result = ESP_OK;

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 2,
        .allocation_unit_size = 4096,
    };
    result = esp_vfs_fat_spiflash_mount_rw_wl(AUDIO_MOUNT_PATH,
                                               AUDIO_PARTITION_LABEL,
                                               &mount_config,
                                               &wl_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "FATFS mount failed: %s", esp_err_to_name(result));
        goto cleanup;
    }
    filesystem_mounted = true;

    audio_file = fopen(AUDIO_FILE_PATH, "rb");
    if (audio_file == NULL) {
        ESP_LOGE(TAG, "missing %s", AUDIO_FILE_PATH);
        result = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }
    if ((fseek(audio_file, 0L, SEEK_END) != 0) ||
        (ftell(audio_file) <= 0L)) {
        result = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    const long file_size = ftell(audio_file);
    if ((file_size <= 0L) || (fseek(audio_file, 0L, SEEK_SET) != 0)) {
        result = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
    ESP_LOGI(TAG, "opening %s (%ld bytes)", AUDIO_FILE_PATH, file_size);

    result = audio_mp3_decoder_init(&s_decoder);
    if (result != ESP_OK) {
        goto cleanup;
    }
    decoder_initialized = true;

    Max98357aDirectConfig amplifier_config =
        max98357a_direct_config_default();
    result = audio_mixer_init(&s_mixer, 0U);
    if (result != ESP_OK) {
        goto cleanup;
    }
    result = audio_mixer_set_volume(&s_mixer,
                                    PLAYBACK_VOLUME_PERMILLE,
                                    PLAYBACK_VOLUME_RAMP_FRAMES);
    if (result != ESP_OK) {
        goto cleanup;
    }
    result = max98357a_direct_init(&s_amplifier, &amplifier_config);
    if (result != ESP_OK) {
        goto cleanup;
    }
    amplifier_initialized = true;
    result = max98357a_direct_start(&s_amplifier);
    if (result != ESP_OK) {
        goto cleanup;
    }
    result = decode_and_play(audio_file, file_size);

cleanup:
    if (amplifier_initialized) {
        esp_err_t cleanup_error = max98357a_direct_deinit(&s_amplifier);
        if ((cleanup_error != ESP_OK) && (result == ESP_OK)) {
            result = cleanup_error;
        }
    }
    if (decoder_initialized) {
        esp_err_t cleanup_error = audio_mp3_decoder_deinit(&s_decoder);
        if ((cleanup_error != ESP_OK) && (result == ESP_OK)) {
            result = cleanup_error;
        }
    }
    if (audio_file != NULL) {
        fclose(audio_file);
    }
    if (filesystem_mounted) {
        esp_err_t cleanup_error = esp_vfs_fat_spiflash_unmount_rw_wl(
            AUDIO_MOUNT_PATH, wl_handle);
        if ((cleanup_error != ESP_OK) && (result == ESP_OK)) {
            result = cleanup_error;
        }
    }

    if (result == ESP_OK) {
        ESP_LOGI(TAG, "test finished");
    } else {
        ESP_LOGE(TAG, "test stopped: %s", esp_err_to_name(result));
    }
}
