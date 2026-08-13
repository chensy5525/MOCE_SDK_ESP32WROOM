#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "msm261.h"

#define MSM261_EXAMPLE_TAG                 "MSM261_EXAMPLE"
#define MSM261_ERROR_LOG_COOLDOWN_MS       1000U

void app_main(void)
{
    msm261_cfg_t config;
    msm261_handle_t microphone = NULL;
    msm261_level_t level;
    TickType_t last_error_log = 0U;
    int result;

    msm261_default_config(&config);
    result = msm261_init(&microphone, &config);
    if (result != 0) {
        printf("[ERR][" MSM261_EXAMPLE_TAG "] init FAIL err=%d\n", result);
        return;
    }

    while (true) {
        result = msm261_read_level(microphone, &level);
        if (result == 0) {
            printf("[INF][" MSM261_EXAMPLE_TAG "] valid=1 peak=%d rms=%.1f dbfs=%.1f dc=%d clipped=%u ok=%u err=%u\n",
                   level.peak, (double)level.rms, (double)level.dbfs,
                   level.dc_mean, (unsigned)level.clipped_samples,
                   (unsigned)level.ok_count, (unsigned)level.error_count);
        } else {
            TickType_t now = xTaskGetTickCount();
            if (last_error_log == 0U ||
                now - last_error_log >=
                    pdMS_TO_TICKS(MSM261_ERROR_LOG_COOLDOWN_MS)) {
                printf("[WRN][" MSM261_EXAMPLE_TAG "] read FAIL err=%d\n",
                       result);
                last_error_log = now;
            }
        }
    }

}
