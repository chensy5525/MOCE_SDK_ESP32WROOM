#include "syn6288e_direct.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SYN6288E_EXAMPLE_DURATION_MS 60000U

void app_main(void)
{
    static const uint8_t danger_text_gbk[] = {
        0xCEU, 0xA3U, 0xCFU, 0xD5U,
    };
    syn6288e_direct_config_t config = SYN6288E_DIRECT_CONFIG_DEFAULT();
    static syn6288e_direct_t speech;
    int result;

    result = syn6288e_direct_init(&speech, &config);
    if (result != 0) {
        printf("[ERR][SYN6288E_DIRECT_EXAMPLE] init failed err=%d\n", result);
        return;
    }

    result = syn6288e_direct_speak_gbk(
        &speech, danger_text_gbk, sizeof(danger_text_gbk));
    if (result != 0) {
        printf("[WRN][SYN6288E_DIRECT_EXAMPLE] speak danger failed err=%d\n",
               result);
    } else {
        printf("[INF][SYN6288E_DIRECT_EXAMPLE] frame transmit complete\n");
        printf("[INF][SYN6288E_DIRECT_EXAMPLE] observing for %u ms\n",
               SYN6288E_EXAMPLE_DURATION_MS);
        vTaskDelay(pdMS_TO_TICKS(SYN6288E_EXAMPLE_DURATION_MS));
    }
    if (syn6288e_direct_deinit(&speech) != 0) {
        printf("[ERR][SYN6288E_DIRECT_EXAMPLE] deinit failed\n");
    }
    printf("[INF][SYN6288E_DIRECT_EXAMPLE] test complete\n");
}
