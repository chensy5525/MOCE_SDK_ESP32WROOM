#include "bsp_i2s.h"

#include <string.h>

#include "board.h"
#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "bsp_i2s";

static i2s_chan_handle_t s_rx_handle;
static bool s_initialized;
static bool s_running;

static TickType_t timeout_ms_to_ticks(uint32_t timeout_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms > 0U) && (ticks == 0U)) {
        ticks = 1U;
    }
    return ticks;
}

static bool pdm_slot_is_valid(bsp_i2s_pdm_slot_t slot)
{
    switch (slot) {
        case BSP_I2S_PDM_SLOT_LEFT:
        case BSP_I2S_PDM_SLOT_RIGHT:
            return true;
        default:
            return false;
    }
}

static bool pdm_rx_config_is_valid(const bsp_i2s_pdm_rx_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->clk_gpio) ||
        !GPIO_IS_VALID_GPIO(config->data_gpio) ||
        (config->clk_gpio == config->data_gpio) ||
        (config->sample_rate_hz == 0U)) {
        return false;
    }
    if ((config->downsample != BSP_I2S_PDM_DOWNSAMPLE_64) &&
        (config->downsample != BSP_I2S_PDM_DOWNSAMPLE_128)) {
        return false;
    }
    return pdm_slot_is_valid(config->slot);
}

esp_err_t bsp_i2s_pdm_rx_init(const bsp_i2s_pdm_rx_config_t *config)
{
    if (!pdm_rx_config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t error = i2s_new_channel(&channel_config, NULL, &s_rx_handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(error));
        return error;
    }

    i2s_pdm_rx_slot_config_t slot_config =
        I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                I2S_SLOT_MODE_MONO);
    slot_config.slot_mask = (config->slot == BSP_I2S_PDM_SLOT_RIGHT)
                                ? I2S_PDM_SLOT_RIGHT
                                : I2S_PDM_SLOT_LEFT;

    i2s_pdm_rx_config_t pdm_config = {0};
    pdm_config.clk_cfg = (i2s_pdm_rx_clk_config_t)
        I2S_PDM_RX_CLK_DEFAULT_CONFIG(config->sample_rate_hz);
    pdm_config.clk_cfg.dn_sample_mode =
        (config->downsample == BSP_I2S_PDM_DOWNSAMPLE_128)
            ? I2S_PDM_DSR_16S
            : I2S_PDM_DSR_8S;
    pdm_config.slot_cfg = slot_config;
    pdm_config.gpio_cfg.clk = config->clk_gpio;
    pdm_config.gpio_cfg.din = config->data_gpio;
    pdm_config.gpio_cfg.invert_flags.clk_inv = config->invert_clk;

    error = i2s_channel_init_pdm_rx_mode(s_rx_handle, &pdm_config);
    if (error != ESP_OK) {
        esp_err_t cleanup_error = i2s_del_channel(s_rx_handle);
        if (cleanup_error != ESP_OK) {
            ESP_LOGE(TAG,
                     "I2S init failed (%s), channel cleanup also failed (%s)",
                     esp_err_to_name(error),
                     esp_err_to_name(cleanup_error));
        } else {
            ESP_LOGE(TAG,
                     "i2s_channel_init_pdm_rx_mode failed: %s",
                     esp_err_to_name(error));
        }
        s_rx_handle = NULL;
        return error;
    }

    s_initialized = true;
    s_running = false;
    ESP_LOGI(TAG,
             "I2S0 PDM RX initialized: CLK GPIO %d, DATA GPIO %d, PCM %lu Hz",
             config->clk_gpio,
             config->data_gpio,
             (unsigned long)config->sample_rate_hz);
    return ESP_OK;
}

esp_err_t bsp_i2s_pdm_rx_start(void)
{
    if (!s_initialized || (s_rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_enable(s_rx_handle);
    if (error == ESP_OK) {
        s_running = true;
    }
    return error;
}

esp_err_t bsp_i2s_pdm_rx_read(int16_t *samples,
                              size_t sample_capacity,
                              size_t *samples_read,
                              uint32_t timeout_ms)
{
    if ((samples == NULL) || (samples_read == NULL) ||
        (sample_capacity == 0U) ||
        (sample_capacity > (SIZE_MAX / sizeof(samples[0]))) ||
        (timeout_ms > BSP_I2S_TIMEOUT_MAX_MS)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_running || (s_rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_read = 0U;
    *samples_read = 0U;
    esp_err_t error = i2s_channel_read(s_rx_handle,
                                       samples,
                                       sample_capacity * sizeof(samples[0]),
                                       &bytes_read,
                                       timeout_ms_to_ticks(timeout_ms));
    if (error != ESP_OK) {
        return error;
    }
    if ((bytes_read % sizeof(samples[0])) != 0U) {
        return ESP_ERR_INVALID_SIZE;
    }

    *samples_read = bytes_read / sizeof(samples[0]);
    return ESP_OK;
}

esp_err_t bsp_i2s_pdm_rx_stop(void)
{
    if (!s_initialized || (s_rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_disable(s_rx_handle);
    if (error == ESP_OK) {
        s_running = false;
    }
    return error;
}

esp_err_t bsp_i2s_pdm_rx_deinit(void)
{
    if (!s_initialized || (s_rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        esp_err_t error = bsp_i2s_pdm_rx_stop();
        if (error != ESP_OK) {
            return error;
        }
    }

    esp_err_t error = i2s_del_channel(s_rx_handle);
    if (error == ESP_OK) {
        s_rx_handle = NULL;
        s_initialized = false;
        s_running = false;
    }
    return error;
}

bool bsp_i2s_pdm_rx_is_initialized(void)
{
    return s_initialized;
}

bool bsp_i2s_pdm_rx_is_running(void)
{
    return s_running;
}
