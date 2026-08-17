#include <inttypes.h>
#include <stdint.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "msm261dgt003_direct_pdm.h"

#define MICROPHONE_SAMPLE_COUNT 512U
#define MICROPHONE_READ_TIMEOUT_MS 1000U

static const char *TAG = "msm261_pdm_test";

static void log_pcm_level(const int16_t *samples,
                          size_t sample_count,
                          uint32_t block_number)
{
    uint64_t absolute_sum = 0U;
    uint32_t peak = 0U;

    for (size_t index = 0U; index < sample_count; ++index) {
        int32_t sample = samples[index];
        uint32_t magnitude = (sample < 0) ? (uint32_t)(-sample)
                                          : (uint32_t)sample;
        absolute_sum += magnitude;
        if (magnitude > peak) {
            peak = magnitude;
        }
    }

    uint32_t mean_absolute =
        (sample_count == 0U) ? 0U : (uint32_t)(absolute_sum / sample_count);
    ESP_LOGI(TAG,
             "block=%" PRIu32 " samples=%u peak=%" PRIu32
             " mean_abs=%" PRIu32,
             block_number,
             (unsigned)sample_count,
             peak,
             mean_absolute);
}

void app_main(void)
{
    static int16_t samples[MICROPHONE_SAMPLE_COUNT];
    Msm261dgt003DirectPdm microphone = {0};
    Msm261dgt003DirectPdmConfig config;
    uint32_t block_number = 0U;

    ESP_LOGI(TAG, "recipe=msm261dgt003_direct_pdm_test");
    ESP_LOGI(TAG,
             "transport=I2S0 PDM RX, CLK=GPIO%d, DATA=GPIO%d",
             BOARD_I2S_PDM_CLK_GPIO,
             BOARD_I2S_PDM_DATA_GPIO);

    ESP_ERROR_CHECK(msm261dgt003_direct_pdm_config_default(&config));
    config.clk_gpio = BOARD_I2S_PDM_CLK_GPIO;
    config.data_gpio = BOARD_I2S_PDM_DATA_GPIO;

    ESP_ERROR_CHECK(msm261dgt003_direct_pdm_init(&microphone, &config));
    ESP_ERROR_CHECK(msm261dgt003_direct_pdm_start(&microphone));

    while (true) {
        size_t samples_read = 0U;
        esp_err_t error = msm261dgt003_direct_pdm_read(
            &microphone,
            samples,
            MICROPHONE_SAMPLE_COUNT,
            &samples_read,
            MICROPHONE_READ_TIMEOUT_MS);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "PCM read failed: %s", esp_err_to_name(error));
            continue;
        }

        ++block_number;
        log_pcm_level(samples, samples_read, block_number);
    }
}
