#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "syn6288e_direct.h"

#include <stdio.h>

#define SYN6288E_EXAMPLE_BROADCAST_INTERVAL_MS 1000U

void app_main(void)
{
    syn6288e_direct_config_t config = SYN6288E_DIRECT_CONFIG_DEFAULT();
    static syn6288e_direct_t speech;
    TickType_t last_wake_time;
    int result;

    result = syn6288e_direct_init(&speech, &config);
    if (result != 0) {
        printf("[ERR][SYN6288E_DIRECT_EXAMPLE] init failed err=%d\n", result);
        return;
    }

    last_wake_time = xTaskGetTickCount();
    while (true) {
        result = syn6288e_direct_speak_danger(&speech);
        if (result != 0) {
            printf("[WRN][SYN6288E_DIRECT_EXAMPLE] speak danger failed err=%d\n",
                   result);
        }
        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(SYN6288E_EXAMPLE_BROADCAST_INTERVAL_MS));
    }
}
