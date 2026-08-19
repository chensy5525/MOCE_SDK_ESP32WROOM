#include "bsp_i2s.h"

#include <string.h>

#include "board.h"
#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#if BOARD_I2S_PDM_RX_PORT != 0
#error "Classic ESP32 PDM RX is only supported on I2S0"
#endif

#if BOARD_I2S_STD_TX_PORT != 0
#error "This BSP currently binds standard I2S TX to I2S0"
#endif

static const char *TAG = "bsp_i2s";

typedef enum {
    BSP_I2S_MODE_NONE = 0,
    BSP_I2S_MODE_PDM_RX,
    BSP_I2S_MODE_STD_TX,
} BspI2sMode;

static i2s_chan_handle_t s_channel_handle;
static BspI2sMode s_mode;
static bool s_running;

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
    if (s_mode != BSP_I2S_MODE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t error = i2s_new_channel(&channel_config, NULL, &s_channel_handle);
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

    error = i2s_channel_init_pdm_rx_mode(s_channel_handle, &pdm_config);
    if (error != ESP_OK) {
        esp_err_t cleanup_error = i2s_del_channel(s_channel_handle);
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
        s_channel_handle = NULL;
        return error;
    }

    s_mode = BSP_I2S_MODE_PDM_RX;
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
    if ((s_mode != BSP_I2S_MODE_PDM_RX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_enable(s_channel_handle);
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
    if ((s_mode != BSP_I2S_MODE_PDM_RX) || !s_running ||
        (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_read = 0U;
    *samples_read = 0U;
    esp_err_t error = i2s_channel_read(s_channel_handle,
                                       samples,
                                       sample_capacity * sizeof(samples[0]),
                                       &bytes_read,
                                       timeout_ms);
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
    if ((s_mode != BSP_I2S_MODE_PDM_RX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_disable(s_channel_handle);
    if (error == ESP_OK) {
        s_running = false;
    }
    return error;
}

esp_err_t bsp_i2s_pdm_rx_deinit(void)
{
    if ((s_mode != BSP_I2S_MODE_PDM_RX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        esp_err_t error = bsp_i2s_pdm_rx_stop();
        if (error != ESP_OK) {
            return error;
        }
    }

    esp_err_t error = i2s_del_channel(s_channel_handle);
    if (error == ESP_OK) {
        s_channel_handle = NULL;
        s_mode = BSP_I2S_MODE_NONE;
        s_running = false;
    }
    return error;
}

bool bsp_i2s_pdm_rx_is_initialized(void)
{
    return s_mode == BSP_I2S_MODE_PDM_RX;
}

bool bsp_i2s_pdm_rx_is_running(void)
{
    return (s_mode == BSP_I2S_MODE_PDM_RX) && s_running;
}

static bool std_tx_config_is_valid(const bsp_i2s_std_tx_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->bclk_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->ws_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config->data_gpio) ||
        (config->bclk_gpio == config->ws_gpio) ||
        (config->bclk_gpio == config->data_gpio) ||
        (config->ws_gpio == config->data_gpio) ||
        (config->sample_rate_hz == 0U)) {
        return false;
    }
    return (config->dma_desc_num >= 2U) &&
           (config->dma_frame_num > 0U) &&
           (config->dma_frame_num <= 1023U);
}

esp_err_t bsp_i2s_std_tx_init(const bsp_i2s_std_tx_config_t *config)
{
    if (!std_tx_config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mode != BSP_I2S_MODE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = config->dma_desc_num;
    channel_config.dma_frame_num = config->dma_frame_num;
    channel_config.auto_clear_after_cb = true;

    esp_err_t error = i2s_new_channel(&channel_config, &s_channel_handle, NULL);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "standard TX channel creation failed: %s",
                 esp_err_to_name(error));
        return error;
    }

    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->bclk_gpio,
            .ws = config->ws_gpio,
            .dout = config->data_gpio,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    error = i2s_channel_init_std_mode(s_channel_handle, &std_config);
    if (error != ESP_OK) {
        esp_err_t cleanup_error = i2s_del_channel(s_channel_handle);
        if (cleanup_error != ESP_OK) {
            ESP_LOGE(TAG, "standard TX init failed (%s), cleanup failed (%s)",
                     esp_err_to_name(error), esp_err_to_name(cleanup_error));
        } else {
            ESP_LOGE(TAG, "standard TX init failed: %s", esp_err_to_name(error));
        }
        s_channel_handle = NULL;
        return error;
    }

    s_mode = BSP_I2S_MODE_STD_TX;
    s_running = false;
    ESP_LOGI(TAG, "I2S0 standard TX: BCLK GPIO %d, WS GPIO %d, DATA GPIO %d, %lu Hz",
             config->bclk_gpio, config->ws_gpio, config->data_gpio,
             (unsigned long)config->sample_rate_hz);
    return ESP_OK;
}

esp_err_t bsp_i2s_std_tx_start(void)
{
    if ((s_mode != BSP_I2S_MODE_STD_TX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_enable(s_channel_handle);
    if (error == ESP_OK) {
        s_running = true;
    }
    return error;
}

esp_err_t bsp_i2s_std_tx_write(const int16_t *stereo_samples,
                              size_t frame_count,
                              size_t *frames_written,
                              uint32_t timeout_ms)
{
    const size_t bytes_per_frame = 2U * sizeof(stereo_samples[0]);
    if ((stereo_samples == NULL) || (frames_written == NULL) ||
        (frame_count == 0U) || (frame_count > (SIZE_MAX / bytes_per_frame)) ||
        (timeout_ms > BSP_I2S_TIMEOUT_MAX_MS)) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((s_mode != BSP_I2S_MODE_STD_TX) || !s_running ||
        (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_written = 0U;
    *frames_written = 0U;
    esp_err_t error = i2s_channel_write(s_channel_handle,
                                        stereo_samples,
                                        frame_count * bytes_per_frame,
                                        &bytes_written,
                                        timeout_ms);
    if (error != ESP_OK) {
        return error;
    }
    if ((bytes_written % bytes_per_frame) != 0U) {
        return ESP_ERR_INVALID_SIZE;
    }
    *frames_written = bytes_written / bytes_per_frame;
    return ESP_OK;
}

esp_err_t bsp_i2s_std_tx_stop(void)
{
    if ((s_mode != BSP_I2S_MODE_STD_TX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_running) {
        return ESP_OK;
    }

    esp_err_t error = i2s_channel_disable(s_channel_handle);
    if (error == ESP_OK) {
        s_running = false;
    }
    return error;
}

esp_err_t bsp_i2s_std_tx_deinit(void)
{
    if ((s_mode != BSP_I2S_MODE_STD_TX) || (s_channel_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        esp_err_t error = bsp_i2s_std_tx_stop();
        if (error != ESP_OK) {
            return error;
        }
    }

    esp_err_t error = i2s_del_channel(s_channel_handle);
    if (error == ESP_OK) {
        s_channel_handle = NULL;
        s_mode = BSP_I2S_MODE_NONE;
        s_running = false;
    }
    return error;
}

bool bsp_i2s_std_tx_is_initialized(void)
{
    return s_mode == BSP_I2S_MODE_STD_TX;
}

bool bsp_i2s_std_tx_is_running(void)
{
    return (s_mode == BSP_I2S_MODE_STD_TX) && s_running;
}
