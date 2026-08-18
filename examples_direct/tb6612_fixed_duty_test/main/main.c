#include "board.h"
#include "esp_log.h"

#define TB6612_TEST_RECIPE_ID  "tb6612_fixed_duty_test"

static const char *TAG = "tb6612_test";

void app_main(void)
{
    ESP_LOGI(TAG, "recipe=%s", TB6612_TEST_RECIPE_ID);
    ESP_LOGI(TAG, "selected_devices=tb6612fng_dual_motor_driver");
    ESP_LOGI(TAG,
             "binding=PWMA:%d AIN1:%d AIN2:%d PWMB:%d BIN1:%d BIN2:%d",
             BOARD_MOTOR_LEFT_PWM_GPIO,
             BOARD_MOTOR_LEFT_IN1_GPIO,
             BOARD_MOTOR_LEFT_IN2_GPIO,
             BOARD_MOTOR_RIGHT_PWM_GPIO,
             BOARD_MOTOR_RIGHT_IN1_GPIO,
             BOARD_MOTOR_RIGHT_IN2_GPIO);
    ESP_LOGE(TAG,
             "hardware_blocked=TB6612_U1_STBY_pin_19_unconnected");
    ESP_LOGE(TAG,
             "no PWM or direction GPIO was configured; no motor command was issued");
}
