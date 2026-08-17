#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NATIVE_PDM_TRANSPORT_TIMEOUT_MAX_MS 60000U

typedef enum {
    NATIVE_PDM_TRANSPORT_SELECT_LOW = 0,
    NATIVE_PDM_TRANSPORT_SELECT_HIGH,
} NativePdmTransportChannel;

typedef enum {
    NATIVE_PDM_TRANSPORT_DOWNSAMPLE_64 = 64,
    NATIVE_PDM_TRANSPORT_DOWNSAMPLE_128 = 128,
} NativePdmTransportDownsample;

typedef struct {
    gpio_num_t clk_gpio;
    gpio_num_t data_gpio;
    uint32_t sample_rate_hz;
    NativePdmTransportDownsample downsample;
    NativePdmTransportChannel channel;
    bool invert_clk;
} NativePdmTransportConfig;

typedef struct {
    void *rx_handle;
    bool initialized;
    bool running;
} NativePdmTransport;

esp_err_t native_pdm_transport_init(
    NativePdmTransport *transport,
    const NativePdmTransportConfig *config);
esp_err_t native_pdm_transport_start(NativePdmTransport *transport);
esp_err_t native_pdm_transport_read(NativePdmTransport *transport,
                                    int16_t *samples,
                                    size_t sample_capacity,
                                    size_t *samples_read,
                                    uint32_t timeout_ms);
esp_err_t native_pdm_transport_stop(NativePdmTransport *transport);
esp_err_t native_pdm_transport_deinit(NativePdmTransport *transport);

#ifdef __cplusplus
}
#endif
