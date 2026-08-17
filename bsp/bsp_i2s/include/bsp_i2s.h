#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_I2S_TIMEOUT_MAX_MS 60000U

typedef enum {
    BSP_I2S_PDM_SLOT_LEFT = 0,
    BSP_I2S_PDM_SLOT_RIGHT,
} bsp_i2s_pdm_slot_t;

typedef enum {
    BSP_I2S_PDM_DOWNSAMPLE_64 = 64,
    BSP_I2S_PDM_DOWNSAMPLE_128 = 128,
} bsp_i2s_pdm_downsample_t;

typedef struct {
    gpio_num_t clk_gpio;
    gpio_num_t data_gpio;
    uint32_t sample_rate_hz;
    bsp_i2s_pdm_downsample_t downsample;
    bsp_i2s_pdm_slot_t slot;
    bool invert_clk;
} bsp_i2s_pdm_rx_config_t;

esp_err_t bsp_i2s_pdm_rx_init(const bsp_i2s_pdm_rx_config_t *config);
esp_err_t bsp_i2s_pdm_rx_start(void);
esp_err_t bsp_i2s_pdm_rx_read(int16_t *samples,
                              size_t sample_capacity,
                              size_t *samples_read,
                              uint32_t timeout_ms);
esp_err_t bsp_i2s_pdm_rx_stop(void);
esp_err_t bsp_i2s_pdm_rx_deinit(void);
bool bsp_i2s_pdm_rx_is_initialized(void);
bool bsp_i2s_pdm_rx_is_running(void);

#ifdef __cplusplus
}
#endif
