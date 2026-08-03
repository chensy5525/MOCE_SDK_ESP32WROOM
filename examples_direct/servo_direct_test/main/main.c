#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "servo_driver.h"

#define SERVO_DIRECT_TEST_STEP_DELAY_MS 1000U

static const char *TAG = "servo_direct_test";

void app_main(void)
{
    static const uint16_t sweep_angles[] = {0U, 90U, 180U, 90U};
    size_t sweep_index = 0U;
    servo_driver_config_t config;

    servo_driver_default_config(&config);

    ESP_LOGI(TAG, "ESP32-WROOM servo_direct_test");
    printf("\r\n==== ESP32-WROOM servo_direct_test ====\r\n");
    printf("direct PWM, no CAN, no CH32\r\n");
    printf("ch0=PWMB1/GPIO%d ch1=PWMB2/GPIO%d ch2=PWMB3/GPIO%d ch3=PWMB4/GPIO%d\r\n",
           BOARD_PWM_B1_GPIO, BOARD_PWM_B2_GPIO, BOARD_PWM_B3_GPIO, BOARD_PWM_B4_GPIO);
    printf("servo PWM: %" PRIu32 "Hz, %" PRIu32 "us..%" PRIu32 "us..%" PRIu32 "us\r\n\r\n",
           config.frequency_hz,
           config.min_pulse_us,
           config.center_pulse_us,
           config.max_pulse_us);

    ESP_ERROR_CHECK(servo_driver_init(&config));

    while (true) {
        uint16_t angle = sweep_angles[sweep_index];
        sweep_index = (sweep_index + 1U) % (sizeof(sweep_angles) / sizeof(sweep_angles[0]));

        ESP_ERROR_CHECK(servo_driver_set_all_angle(angle));

        printf("servo_direct_test angle=%u ch0=PWMB1 ch1=PWMB2 ch2=PWMB3 ch3=PWMB4\r\n",
               angle);
        vTaskDelay(pdMS_TO_TICKS(SERVO_DIRECT_TEST_STEP_DELAY_MS));
    }
}
