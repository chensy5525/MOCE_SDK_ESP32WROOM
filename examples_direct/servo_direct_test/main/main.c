#include <stddef.h>

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "servo_driver.h"

#define SERVO_TEST_RECIPE_ID             "mg90s_dual_arbitrary_angle_test"
#define SERVO_TEST_CHANNEL_0             0U
#define SERVO_TEST_CHANNEL_1             1U
#define SERVO_TEST_CHANNEL_COUNT         2U
#define SERVO_TEST_POSITION_HOLD_MS      3000U
#define ARRAY_SIZE(array)                (sizeof(array) / sizeof((array)[0]))

_Static_assert(BOARD_SERVO_COUNT == SERVO_TEST_CHANNEL_COUNT,
               "servo_direct_test requires the two-channel board binding");

static const char *TAG = "servo_direct_test";

static const uint16_t TEST_ANGLES_DEG[] = {
    0U,
    17U,
    63U,
    91U,
    127U,
    180U,
    90U,
};

static esp_err_t run_angle_test(ServoDriver *driver)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t index = 0U;
         index < ARRAY_SIZE(TEST_ANGLES_DEG);
         ++index) {
        uint16_t angle_degrees = TEST_ANGLES_DEG[index];
        esp_err_t error = servo_driver_set_all_angles(driver, angle_degrees);
        if (error != ESP_OK) {
            ESP_LOGE(TAG,
                     "dual angle=%u command failed: %s",
                     (unsigned)angle_degrees,
                     esp_err_to_name(error));
            return error;
        }

        ESP_LOGI(TAG,
                 "angle=%u commanded on both channels; observe both servos",
                 (unsigned)angle_degrees);
        vTaskDelay(pdMS_TO_TICKS(SERVO_TEST_POSITION_HOLD_MS));
    }

    return ESP_OK;
}

static void enter_safe_state(ServoDriver *driver)
{
    if (driver == NULL) {
        return;
    }

    if (driver->initialized) {
        esp_err_t error = servo_driver_set_all_angles(driver, 90U);
        if (error == ESP_OK) {
            ESP_LOGW(TAG,
                     "safe state applied: both channels at nominal 90 degrees");
            return;
        }
        ESP_LOGE(TAG,
                 "dual safe-position command failed: %s",
                 esp_err_to_name(error));
    }

    if (driver->initialized || driver->cleanup_required) {
        esp_err_t error = servo_driver_deinit(driver);
        if (error != ESP_OK) {
            ESP_LOGE(TAG,
                     "dual PWM shutdown incomplete: %s; disconnect servo power",
                     esp_err_to_name(error));
            return;
        }
    }

    ESP_LOGW(TAG, "safe state applied: both PWM outputs disabled");
}

void app_main(void)
{
    ServoDriver driver = SERVO_DRIVER_INITIALIZER;
    ServoDriverConfig config = {0};

    esp_err_t error = servo_driver_config_default(&config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "default configuration failed: %s", esp_err_to_name(error));
        return;
    }

    config.channel_count = SERVO_TEST_CHANNEL_COUNT;
    config.channels[SERVO_TEST_CHANNEL_0].gpio = BOARD_SERVO_GPIO_0;
    config.channels[SERVO_TEST_CHANNEL_0].ledc_channel =
        BOARD_SERVO_PWM_CHANNEL_0;
    config.channels[SERVO_TEST_CHANNEL_1].gpio = BOARD_SERVO_GPIO_1;
    config.channels[SERVO_TEST_CHANNEL_1].ledc_channel =
        BOARD_SERVO_PWM_CHANNEL_1;
    config.speed_mode = BOARD_SERVO_PWM_MODE;
    config.timer = BOARD_SERVO_PWM_TIMER;
    config.duty_resolution = BOARD_SERVO_PWM_DUTY_RES;
    config.frequency_hz = BOARD_SERVO_PWM_FREQUENCY_HZ;

    ESP_LOGI(TAG, "recipe=%s", SERVO_TEST_RECIPE_ID);
    ESP_LOGI(TAG, "selected_devices=mg90s_servo instances=2");
    ESP_LOGI(TAG,
             "transport=esp32_native_pwm pwm1_gpio=%d pwm2_gpio=%d frequency=%uHz",
             config.channels[SERVO_TEST_CHANNEL_0].gpio,
             config.channels[SERVO_TEST_CHANNEL_1].gpio,
             (unsigned)config.frequency_hz);
    ESP_LOGI(TAG, "test sequence=0,17,63,91,127,180,90 degrees");

    error = servo_driver_init(&driver, &config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "dual servo initialization failed: %s",
                 esp_err_to_name(error));
        enter_safe_state(&driver);
        return;
    }

    error = run_angle_test(&driver);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "test aborted; entering the defined safe state");
        enter_safe_state(&driver);
        return;
    }

    ESP_LOGI(TAG,
             "test complete; both final commands=90; no further switching");
}
