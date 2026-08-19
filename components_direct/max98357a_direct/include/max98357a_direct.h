#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX98357A_DIRECT_DEFAULT_RATE_HZ    48000U
#define MAX98357A_DIRECT_DEFAULT_TIMEOUT_MS 250U

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t write_timeout_ms;
} Max98357aDirectConfig;

typedef struct {
    Max98357aDirectConfig config;
    bool initialized;
    bool running;
} Max98357aDirect;

Max98357aDirectConfig max98357a_direct_config_default(void);
esp_err_t max98357a_direct_init(Max98357aDirect *device,
                               const Max98357aDirectConfig *config);
esp_err_t max98357a_direct_start(Max98357aDirect *device);
esp_err_t max98357a_direct_write_pcm(Max98357aDirect *device,
                                    const int16_t *interleaved_stereo_samples,
                                    size_t frame_count);
esp_err_t max98357a_direct_stop(Max98357aDirect *device);
esp_err_t max98357a_direct_deinit(Max98357aDirect *device);

#ifdef __cplusplus
}
#endif
