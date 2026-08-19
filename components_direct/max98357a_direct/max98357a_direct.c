#include "max98357a_direct.h"

#include <string.h>

#include "board.h"
#include "bsp_i2s.h"

#define MAX98357A_MIN_SAMPLE_RATE_HZ 8000U
#define MAX98357A_MAX_SAMPLE_RATE_HZ 96000U
#define MAX98357A_DMA_DESC_NUM        6U
#define MAX98357A_DMA_FRAME_NUM       240U

static bool config_is_valid(const Max98357aDirectConfig *config)
{
    return (config != NULL) &&
           (config->sample_rate_hz >= MAX98357A_MIN_SAMPLE_RATE_HZ) &&
           (config->sample_rate_hz <= MAX98357A_MAX_SAMPLE_RATE_HZ) &&
           (config->write_timeout_ms > 0U) &&
           (config->write_timeout_ms <= BSP_I2S_TIMEOUT_MAX_MS);
}

Max98357aDirectConfig max98357a_direct_config_default(void)
{
    return (Max98357aDirectConfig){
        .sample_rate_hz = MAX98357A_DIRECT_DEFAULT_RATE_HZ,
        .write_timeout_ms = MAX98357A_DIRECT_DEFAULT_TIMEOUT_MS,
    };
}

esp_err_t max98357a_direct_init(Max98357aDirect *device,
                               const Max98357aDirectConfig *config)
{
    if ((device == NULL) || !config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    const bsp_i2s_std_tx_config_t i2s_config = {
        .bclk_gpio = BOARD_I2S_STD_TX_BCLK_GPIO,
        .ws_gpio = BOARD_I2S_STD_TX_WS_GPIO,
        .data_gpio = BOARD_I2S_STD_TX_DATA_GPIO,
        .sample_rate_hz = config->sample_rate_hz,
        .dma_desc_num = MAX98357A_DMA_DESC_NUM,
        .dma_frame_num = MAX98357A_DMA_FRAME_NUM,
    };
    esp_err_t error = bsp_i2s_std_tx_init(&i2s_config);
    if (error != ESP_OK) {
        return error;
    }

    memset(device, 0, sizeof(*device));
    device->config = *config;
    device->initialized = true;
    return ESP_OK;
}

esp_err_t max98357a_direct_start(Max98357aDirect *device)
{
    if ((device == NULL) || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (device->running) {
        return ESP_OK;
    }
    esp_err_t error = bsp_i2s_std_tx_start();
    if (error == ESP_OK) {
        device->running = true;
    }
    return error;
}

esp_err_t max98357a_direct_write_pcm(Max98357aDirect *device,
                                    const int16_t *interleaved_stereo_samples,
                                    size_t frame_count)
{
    if ((device == NULL) || !device->initialized || !device->running) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((interleaved_stereo_samples == NULL) || (frame_count == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t frames_written = 0U;
    esp_err_t error = bsp_i2s_std_tx_write(interleaved_stereo_samples,
                                          frame_count,
                                          &frames_written,
                                          device->config.write_timeout_ms);
    if (error != ESP_OK) {
        return error;
    }
    return (frames_written == frame_count) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t max98357a_direct_stop(Max98357aDirect *device)
{
    if ((device == NULL) || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!device->running) {
        return ESP_OK;
    }

    esp_err_t error = bsp_i2s_std_tx_stop();
    if (error == ESP_OK) {
        device->running = false;
    }
    return error;
}

esp_err_t max98357a_direct_deinit(Max98357aDirect *device)
{
    if ((device == NULL) || !device->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (device->running) {
        esp_err_t error = max98357a_direct_stop(device);
        if (error != ESP_OK) {
            return error;
        }
    }
    esp_err_t error = bsp_i2s_std_tx_deinit();
    if (error == ESP_OK) {
        memset(device, 0, sizeof(*device));
    }
    return error;
}
