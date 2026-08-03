#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "syn6288_direct.h"

void app_main(void)
{
    ESP_ERROR_CHECK(syn6288_direct_init());
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (true) {
        ESP_ERROR_CHECK(syn6288_direct_speak_test());
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
