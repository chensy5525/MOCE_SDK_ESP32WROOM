#include "driver/gpio.h"
#include "driver/ledc.h"
#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tb6612_driver.h"

#define TB6612_TEST_RECIPE_ID          "tb6612_fixed_duty_test"
#define TB6612_TEST_DUTY_PERCENT       70U
#define TB6612_TEST_RUN_TIME_MS        5000U

static const char *TAG = "tb6612_test";

static void stop_and_deinit(Tb6612Driver *driver)
{
    esp_err_t error = tb6612_driver_deinit(driver);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "safe shutdown incomplete: %s", esp_err_to_name(error));
    }
}

void app_main(void)
{
    Tb6612Driver driver = TB6612_DRIVER_INITIALIZER;
    Tb6612DriverConfig config = {0};

    esp_err_t error = tb6612_driver_config_default(&config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "default configuration failed: %s", esp_err_to_name(error));
        return;
    }

    config.speed_mode = BOARD_MOTOR_PWM_MODE;
    config.frequency_hz = BOARD_MOTOR_PWM_FREQUENCY_HZ;
    config.duty_resolution = BOARD_MOTOR_PWM_DUTY_RES;
    config.timer = BOARD_MOTOR_PWM_TIMER;
    config.channels[TB6612_MOTOR_A] = (Tb6612ChannelConfig) {
        .pwm_gpio = BOARD_MOTOR_LEFT_PWM_GPIO,
        .in1_gpio = BOARD_MOTOR_LEFT_IN1_GPIO,
        .in2_gpio = BOARD_MOTOR_LEFT_IN2_GPIO,
        .pwm_channel = BOARD_MOTOR_LEFT_PWM_CHANNEL,
    };
    config.channels[TB6612_MOTOR_B] = (Tb6612ChannelConfig) {
        .pwm_gpio = BOARD_MOTOR_RIGHT_PWM_GPIO,
        .in1_gpio = BOARD_MOTOR_RIGHT_IN1_GPIO,
        .in2_gpio = BOARD_MOTOR_RIGHT_IN2_GPIO,
        .pwm_channel = BOARD_MOTOR_RIGHT_PWM_CHANNEL,
    };
    config.control_stby = false;

    ESP_LOGI(TAG, "recipe=%s", TB6612_TEST_RECIPE_ID);
    ESP_LOGI(TAG, "selected_devices=tb6612fng_dual_motor_driver");
    ESP_LOGI(TAG,
             "transport=esp32_native_pwm_gpio frequency=%uHz duty=%u%%",
             (unsigned)config.frequency_hz,
             (unsigned)TB6612_TEST_DUTY_PERCENT);
    ESP_LOGW(TAG, "STBY is not controlled; external active-high wiring required");

    error = tb6612_driver_init(&driver, &config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "driver initialization failed: %s", esp_err_to_name(error));
        return;
    }

    error = tb6612_driver_set_output(&driver,
                                     TB6612_MOTOR_A,
                                     TB6612_DIRECTION_FORWARD,
                                     TB6612_TEST_DUTY_PERCENT);
    if (error == ESP_OK) {
        error = tb6612_driver_set_output(&driver,
                                         TB6612_MOTOR_B,
                                         TB6612_DIRECTION_FORWARD,
                                         TB6612_TEST_DUTY_PERCENT);
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "motor command failed: %s", esp_err_to_name(error));
        stop_and_deinit(&driver);
        return;
    }

    ESP_LOGI(TAG, "both motors commanded forward at fixed duty");
    vTaskDelay(pdMS_TO_TICKS(TB6612_TEST_RUN_TIME_MS));

    error = tb6612_driver_stop_all(&driver);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "motor stop failed: %s", esp_err_to_name(error));
    } else {
        ESP_LOGI(TAG, "test complete; both motor commands stopped");
    }
    stop_and_deinit(&driver);
}
