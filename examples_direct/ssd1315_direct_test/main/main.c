#include "ssd1315.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1315_EXAMPLE_TEXT_X       0U
#define SSD1315_EXAMPLE_TEXT_PAGE    0U

void app_main(void)
{
    const ssd1315_cfg_t config = {
        .i2c_addr = SSD1315_I2C_ADDR,
        .clk_speed_hz = SSD1315_I2C_FREQ_HZ,
        .initialize_i2c = true,
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

    if (result != 0 && display != NULL) {
        (void)ssd1315_deinit_device(display);
    }

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}
