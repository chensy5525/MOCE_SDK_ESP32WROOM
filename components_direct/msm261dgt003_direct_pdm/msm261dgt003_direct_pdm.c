#include "msm261dgt003_direct_pdm.h"

#include <inttypes.h>

#include "esp_log.h"

#define MSM261DGT003_DIRECT_PDM_DOWNSAMPLE_64  64U
#define MSM261DGT003_DIRECT_PDM_DOWNSAMPLE_128 128U

static const char *TAG = "msm261dgt003";

static bool channel_is_valid(Msm261dgt003DirectPdmChannel channel)
{
    switch (channel) {
        case MSM261DGT003_DIRECT_PDM_SELECT_LOW:
        case MSM261DGT003_DIRECT_PDM_SELECT_HIGH:
            return true;
        default:
            return false;
    }
}

static bool config_is_valid(const Msm261dgt003DirectPdmConfig *config)
{
    if (config == NULL) {
        return false;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->clk_gpio) ||
        !GPIO_IS_VALID_GPIO(config->data_gpio) ||
        (config->clk_gpio == config->data_gpio)) {
        return false;
    }
    if ((config->sample_rate_hz <
         MSM261DGT003_DIRECT_PDM_MIN_SAMPLE_RATE_HZ) ||
        (config->sample_rate_hz >
         MSM261DGT003_DIRECT_PDM_MAX_SAMPLE_RATE_HZ)) {
        return false;
    }
    return channel_is_valid(config->channel);
}

static NativePdmTransportDownsample select_downsample(
    uint32_t sample_rate_hz)
{
    uint32_t clock_hz =
        sample_rate_hz * MSM261DGT003_DIRECT_PDM_DOWNSAMPLE_64;
    return (clock_hz < MSM261DGT003_DIRECT_PDM_STANDARD_CLK_MIN_HZ)
               ? NATIVE_PDM_TRANSPORT_DOWNSAMPLE_128
               : NATIVE_PDM_TRANSPORT_DOWNSAMPLE_64;
}

esp_err_t msm261dgt003_direct_pdm_config_default(
    Msm261dgt003DirectPdmConfig *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *config = (Msm261dgt003DirectPdmConfig) {
        .clk_gpio = GPIO_NUM_NC,
        .data_gpio = GPIO_NUM_NC,
        .sample_rate_hz =
            MSM261DGT003_DIRECT_PDM_DEFAULT_SAMPLE_RATE_HZ,
        .channel = MSM261DGT003_DIRECT_PDM_SELECT_LOW,
        .invert_clk = false,
    };
    return ESP_OK;
}

esp_err_t msm261dgt003_direct_pdm_init(
    Msm261dgt003DirectPdm *device,
    const Msm261dgt003DirectPdmConfig *config)
{
    if ((device == NULL) || !config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }

    NativePdmTransportDownsample downsample =
        select_downsample(config->sample_rate_hz);
    uint32_t pdm_clock_hz =
        config->sample_rate_hz * (uint32_t)downsample;
    if ((pdm_clock_hz < MSM261DGT003_DIRECT_PDM_STANDARD_CLK_MIN_HZ) ||
        (pdm_clock_hz > MSM261DGT003_DIRECT_PDM_STANDARD_CLK_MAX_HZ)) {
        return ESP_ERR_INVALID_ARG;
    }

    NativePdmTransportConfig transport_config = {
        .clk_gpio = config->clk_gpio,
        .data_gpio = config->data_gpio,
        .sample_rate_hz = config->sample_rate_hz,
        .downsample = downsample,
        .channel =
            (config->channel == MSM261DGT003_DIRECT_PDM_SELECT_HIGH)
                ? NATIVE_PDM_TRANSPORT_SELECT_HIGH
                : NATIVE_PDM_TRANSPORT_SELECT_LOW,
        .invert_clk = config->invert_clk,
    };

    esp_err_t error =
        native_pdm_transport_init(&device->transport, &transport_config);
    if (error != ESP_OK) {
        return error;
    }

    ESP_LOGI(TAG,
             "PCM=%" PRIu32 " Hz, PDM CLK=%" PRIu32 " Hz, DSR=%u",
             config->sample_rate_hz,
             pdm_clock_hz,
             (unsigned)downsample);
    return ESP_OK;
}

esp_err_t msm261dgt003_direct_pdm_start(Msm261dgt003DirectPdm *device)
{
    return (device == NULL)
               ? ESP_ERR_INVALID_ARG
               : native_pdm_transport_start(&device->transport);
}

esp_err_t msm261dgt003_direct_pdm_read(Msm261dgt003DirectPdm *device,
                                       int16_t *samples,
                                       size_t sample_capacity,
                                       size_t *samples_read,
                                       uint32_t timeout_ms)
{
    return (device == NULL)
               ? ESP_ERR_INVALID_ARG
               : native_pdm_transport_read(&device->transport,
                                           samples,
                                           sample_capacity,
                                           samples_read,
                                           timeout_ms);
}

esp_err_t msm261dgt003_direct_pdm_stop(Msm261dgt003DirectPdm *device)
{
    return (device == NULL)
               ? ESP_ERR_INVALID_ARG
               : native_pdm_transport_stop(&device->transport);
}

esp_err_t msm261dgt003_direct_pdm_deinit(Msm261dgt003DirectPdm *device)
{
    return (device == NULL)
               ? ESP_ERR_INVALID_ARG
               : native_pdm_transport_deinit(&device->transport);
}
