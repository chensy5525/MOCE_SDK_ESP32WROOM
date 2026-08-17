#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSM261DGT003_DIRECT_PDM_DEFAULT_SAMPLE_RATE_HZ 44100U
#define MSM261DGT003_DIRECT_PDM_DEFAULT_TIMEOUT_MS     1000U
#define MSM261DGT003_DIRECT_PDM_MIN_SAMPLE_RATE_HZ     16000U
#define MSM261DGT003_DIRECT_PDM_MAX_SAMPLE_RATE_HZ     48000U
#define MSM261DGT003_DIRECT_PDM_STANDARD_CLK_MIN_HZ    1100000U
#define MSM261DGT003_DIRECT_PDM_STANDARD_CLK_MAX_HZ    4000000U

typedef enum {
    MSM261DGT003_DIRECT_PDM_SELECT_LOW = 0,
    MSM261DGT003_DIRECT_PDM_SELECT_HIGH,
} Msm261dgt003DirectPdmChannel;

typedef struct {
    gpio_num_t clk_gpio;
    gpio_num_t data_gpio;
    uint32_t sample_rate_hz;
    Msm261dgt003DirectPdmChannel channel;
    bool invert_clk;
} Msm261dgt003DirectPdmConfig;

typedef struct {
    bool initialized;
    bool running;
} Msm261dgt003DirectPdm;

esp_err_t msm261dgt003_direct_pdm_config_default(
    Msm261dgt003DirectPdmConfig *config);
esp_err_t msm261dgt003_direct_pdm_init(
    Msm261dgt003DirectPdm *device,
    const Msm261dgt003DirectPdmConfig *config);
esp_err_t msm261dgt003_direct_pdm_start(Msm261dgt003DirectPdm *device);
esp_err_t msm261dgt003_direct_pdm_read(Msm261dgt003DirectPdm *device,
                                       int16_t *samples,
                                       size_t sample_capacity,
                                       size_t *samples_read,
                                       uint32_t timeout_ms);
esp_err_t msm261dgt003_direct_pdm_stop(Msm261dgt003DirectPdm *device);
esp_err_t msm261dgt003_direct_pdm_deinit(Msm261dgt003DirectPdm *device);

#ifdef __cplusplus
}
#endif
