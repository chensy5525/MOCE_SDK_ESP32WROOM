#include <stddef.h>

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "servo_driver.h"

#define SERVO_TEST_RECIPE_ID             "mg90s_one_shot_five_position_test"
#define SERVO_TEST_CHANNEL_INDEX         0U
#define SERVO_TEST_POSITION_HOLD_MS      3000U
#define ARRAY_SIZE(array)                (sizeof(array) / sizeof((array)[0]))

static const char *TAG = "servo_direct_test";

static const ServoPosition TEST_POSITIONS[] = {
    SERVO_POSITION_0_DEG,
    SERVO_POSITION_45_DEG,
    SERVO_POSITION_90_DEG,
    SERVO_POSITION_135_DEG,
    SERVO_POSITION_180_DEG,
    SERVO_POSITION_135_DEG,
    SERVO_POSITION_90_DEG,
};

static esp_err_t run_position_test(ServoDriver *driver)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t index = 0U;
         index < ARRAY_SIZE(TEST_POSITIONS);
         ++index) {
        ServoPosition position = TEST_POSITIONS[index];
        uint16_t degrees = 0U;
        esp_err_t error = servo_driver_position_to_degrees(position, &degrees);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "invalid recipe position at index=%u", (unsigned)index);
            return error;
        }

        error = servo_driver_set_position(driver,
                                          SERVO_TEST_CHANNEL_INDEX,
                                          position);
        if (error != ESP_OK) {
            ESP_LOGE(TAG,
                     "position=%u command failed: %s",
                     (unsigned)degrees,
                     esp_err_to_name(error));
            return error;
        }

        ESP_LOGI(TAG,
                 "position=%u commanded; observe the servo",
                 (unsigned)degrees);
        vTaskDelay(pdMS_TO_TICKS(SERVO_TEST_POSITION_HOLD_MS));
    }

    return ESP_OK;
}

static void enter_safe_state(ServoDriver *driver)
{
    esp_err_t error = servo_driver_set_position(driver,
                                                SERVO_TEST_CHANNEL_INDEX,
                                                SERVO_POSITION_90_DEG);
    if (error == ESP_OK) {
        ESP_LOGW(TAG, "safe state applied: nominal 90-degree command");
        return;
    }

    ESP_LOGE(TAG,
             "safe position command failed: %s; disabling PWM",
             esp_err_to_name(error));
    error = servo_driver_deinit(driver);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "PWM shutdown incomplete: %s", esp_err_to_name(error));
    }
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

    config.channel_count = 1U;
    config.channels[SERVO_TEST_CHANNEL_INDEX].gpio = BOARD_PWM_B1_GPIO;
    config.channels[SERVO_TEST_CHANNEL_INDEX].ledc_channel =
        BOARD_SERVO_PWM_CHANNEL_0;
    config.speed_mode = BOARD_SERVO_PWM_MODE;
    config.timer = BOARD_SERVO_PWM_TIMER;
    config.duty_resolution = LEDC_TIMER_16_BIT;

    ESP_LOGI(TAG, "recipe=%s", SERVO_TEST_RECIPE_ID);
    ESP_LOGI(TAG, "selected_devices=mg90s_servo");
    ESP_LOGI(TAG,
             "transport=esp32_native_pwm gpio=%d frequency=%uHz",
             config.channels[SERVO_TEST_CHANNEL_INDEX].gpio,
             (unsigned)config.frequency_hz);
    ESP_LOGI(TAG, "test sequence=0,45,90,135,180,135,90");

    error = servo_driver_init(&driver, &config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "servo initialization failed: %s", esp_err_to_name(error));
        return;
    }

    error = run_position_test(&driver);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "test aborted; entering the defined safe state");
        enter_safe_state(&driver);
        return;
    }

    ESP_LOGI(TAG, "test complete; final command=90; no further switching");
}
