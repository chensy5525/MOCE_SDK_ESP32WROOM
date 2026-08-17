#include "ssd1315.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1315_EXAMPLE_TEXT_X       0U
#define SSD1315_EXAMPLE_TEXT_PAGE    0U
#define SSD1315_EXAMPLE_DURATION_MS  60000U

void app_main(void)
{
    const ssd1315_cfg_t config = {
        .i2c_addr = SSD1315_I2C_ADDR,
        .clk_speed_hz = SSD1315_I2C_FREQ_HZ,
    };
    ssd1315_handle_t display = NULL;
    int result;

    result = ssd1315_init_device(&display, &config);
    if (result == 0) {
        result = ssd1315_draw_text(display, SSD1315_EXAMPLE_TEXT_X,
                                   SSD1315_EXAMPLE_TEXT_PAGE,
                                   "SSD1315 OK");
    }
    if (result == 0) {
        result = ssd1315_refresh_display(display);
    }

    if (result == 0) {
        printf("[INF][SSD1315_EXAMPLE] display refreshed; observing for %u ms\n",
               SSD1315_EXAMPLE_DURATION_MS);
        vTaskDelay(pdMS_TO_TICKS(SSD1315_EXAMPLE_DURATION_MS));
    } else {
        printf("[ERR][SSD1315_EXAMPLE] operation failed err=%d\n", result);
    }

    if (display != NULL) {
        int deinit_result = ssd1315_deinit_device(display);
        if (deinit_result != 0) {
            printf("[ERR][SSD1315_EXAMPLE] deinit failed err=%d\n",
                   deinit_result);
        }
    }

    printf("[INF][SSD1315_EXAMPLE] example complete\n");
}
