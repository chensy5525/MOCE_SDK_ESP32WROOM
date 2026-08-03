#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_ssd1315_final.h"

static const char *TAG = "oled_direct_test";

static void print_status(const char *stage)
{
    oled_ssd1315_status_t status;
    oled_ssd1315_get_status(&status);
    ESP_LOGI(TAG,
             "%s state=%s addr=0x%02X i2c=%u device=%u init=%u display=%u writes=%lu refresh=%lu errors=%lu last=%s",
             stage,
             oled_ssd1315_state_text(status.state),
             status.address,
             status.i2c_ready ? 1U : 0U,
             status.device_found ? 1U : 0U,
             status.initialized ? 1U : 0U,
             status.display_on ? 1U : 0U,
             (unsigned long)status.write_count,
             (unsigned long)status.refresh_count,
             (unsigned long)status.error_count,
             oled_ssd1315_result_text(status.last_result));
}

void app_main(void)
{
    printf("\n==== ESP32-WROOM oled_direct_test ====\n");
    printf("Connection: GPIO21 SDA -> SSD1315 SDA, GPIO22 SCL -> SSD1315 SCL\n");
    printf("SSD1315: 128x64 addr7=0x3C (fallback 0x3D) I2C=400kHz\n");
    printf("Built-in Chinese glyphs: 你好显示正常\n\n");

    oled_ssd1315_config_t config;
    oled_ssd1315_default_config(&config);

    oled_ssd1315_result_t result;
    do {
        result = oled_ssd1315_init(&config);
        ESP_LOGI(TAG, "oled init result=%s",
                 oled_ssd1315_result_text(result));
        print_status("after_init");
        if (result != OLED_SSD1315_RESULT_OK) {
            ESP_LOGW(TAG,
                     "retry in 3000 ms; check 3.3V/GND/SDA/SCL and addresses 0x3C/0x3D");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    } while (result != OLED_SSD1315_RESULT_OK);

    uint32_t last_status_ms = 0U;
    while (true) {
        uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000LL);

        result = oled_ssd1315_clear();
        if (result == OLED_SSD1315_RESULT_OK) {
            result = oled_ssd1315_draw_utf8(0U, 0U, "你好");
        }
        if (result == OLED_SSD1315_RESULT_OK) {
            result = oled_ssd1315_draw_utf8(0U, 20U, "显示正常");
        }
        if (result == OLED_SSD1315_RESULT_OK) {
            result = oled_ssd1315_draw_ascii(0U, 48U, "UP:");
        }
        if (result == OLED_SSD1315_RESULT_OK) {
            result = oled_ssd1315_draw_uint(18U, 48U, uptime_s);
        }
        if (result == OLED_SSD1315_RESULT_OK) {
            result = oled_ssd1315_refresh();
        }

        if (result != OLED_SSD1315_RESULT_OK) {
            ESP_LOGW(TAG, "oled update result=%s",
                     oled_ssd1315_result_text(result));
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
        if (now_ms - last_status_ms >= 3000U) {
            last_status_ms = now_ms;
            print_status("runtime");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
