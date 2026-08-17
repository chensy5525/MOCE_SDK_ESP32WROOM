#include "native_pdm_transport.h"

#include <string.h>

#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "freertos/FreeRTOS.h"

static TickType_t timeout_ms_to_ticks(uint32_t timeout_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms > 0U) && (ticks == 0U)) {
        ticks = 1U;
    }
    return ticks;
}

static bool config_is_valid(const NativePdmTransportConfig *config)
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
    if ((config->downsample != NATIVE_PDM_TRANSPORT_DOWNSAMPLE_64) &&
        (config->downsample != NATIVE_PDM_TRANSPORT_DOWNSAMPLE_128)) {
        return false;
    }
    switch (config->channel) {
        case NATIVE_PDM_TRANSPORT_SELECT_LOW:
        case NATIVE_PDM_TRANSPORT_SELECT_HIGH:
            return true;
        default:
            return false;
    }
}

esp_err_t native_pdm_transport_init(
    NativePdmTransport *transport,
    const NativePdmTransportConfig *config)
{
    if ((transport == NULL) || !config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (transport->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_handle_t rx_handle = NULL;
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t error = i2s_new_channel(&channel_config, NULL, &rx_handle);
    if (error != ESP_OK) {
        return error;
    }

    i2s_pdm_rx_slot_config_t slot_config =
        I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                I2S_SLOT_MODE_MONO);
    slot_config.slot_mask =
        (config->channel == NATIVE_PDM_TRANSPORT_SELECT_HIGH)
            ? I2S_PDM_SLOT_RIGHT
            : I2S_PDM_SLOT_LEFT;

    i2s_pdm_rx_config_t pdm_config = {0};
    pdm_config.clk_cfg = (i2s_pdm_rx_clk_config_t)
        I2S_PDM_RX_CLK_DEFAULT_CONFIG(config->sample_rate_hz);
    pdm_config.clk_cfg.dn_sample_mode =
        (config->downsample == NATIVE_PDM_TRANSPORT_DOWNSAMPLE_128)
            ? I2S_PDM_DSR_16S
            : I2S_PDM_DSR_8S;
    pdm_config.slot_cfg = slot_config;
    pdm_config.gpio_cfg.clk = config->clk_gpio;
    pdm_config.gpio_cfg.din = config->data_gpio;
    pdm_config.gpio_cfg.invert_flags.clk_inv = config->invert_clk;

    error = i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_config);
    if (error != ESP_OK) {
        (void)i2s_del_channel(rx_handle);
        return error;
    }

    transport->rx_handle = rx_handle;
    transport->initialized = true;
    transport->running = false;
    return ESP_OK;
}

esp_err_t native_pdm_transport_start(NativePdmTransport *transport)
{
    if ((transport == NULL) || !transport->initialized ||
        (transport->rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (transport->running) {
        return ESP_OK;
    }

    esp_err_t error =
        i2s_channel_enable((i2s_chan_handle_t)transport->rx_handle);
    if (error == ESP_OK) {
        transport->running = true;
    }
    return error;
}

esp_err_t native_pdm_transport_read(NativePdmTransport *transport,
                                    int16_t *samples,
                                    size_t sample_capacity,
                                    size_t *samples_read,
                                    uint32_t timeout_ms)
{
    if ((transport == NULL) || (samples == NULL) ||
        (samples_read == NULL) || (sample_capacity == 0U) ||
        (timeout_ms > NATIVE_PDM_TRANSPORT_TIMEOUT_MAX_MS)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!transport->initialized || !transport->running ||
        (transport->rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sample_capacity > (SIZE_MAX / sizeof(samples[0]))) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t bytes_read = 0U;
    *samples_read = 0U;
    esp_err_t error = i2s_channel_read(
        (i2s_chan_handle_t)transport->rx_handle,
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

esp_err_t native_pdm_transport_stop(NativePdmTransport *transport)
{
    if ((transport == NULL) || !transport->initialized ||
        (transport->rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transport->running) {
        return ESP_OK;
    }

    esp_err_t error =
        i2s_channel_disable((i2s_chan_handle_t)transport->rx_handle);
    if (error == ESP_OK) {
        transport->running = false;
    }
    return error;
}

esp_err_t native_pdm_transport_deinit(NativePdmTransport *transport)
{
    if ((transport == NULL) || !transport->initialized ||
        (transport->rx_handle == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (transport->running) {
        esp_err_t error = native_pdm_transport_stop(transport);
        if (error != ESP_OK) {
            return error;
        }
    }

    esp_err_t error =
        i2s_del_channel((i2s_chan_handle_t)transport->rx_handle);
    if (error == ESP_OK) {
        memset(transport, 0, sizeof(*transport));
    }
    return error;
}
