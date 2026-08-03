#include "servo_driver.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "servo_driver";

static const servo_driver_channel_config_t s_default_channels[SERVO_DRIVER_MAX_CHANNELS] = {
    {.name = "PWMB1", .gpio = BOARD_PWM_B1_GPIO, .ledc_channel = LEDC_CHANNEL_1},
    {.name = "PWMB2", .gpio = BOARD_PWM_B2_GPIO, .ledc_channel = LEDC_CHANNEL_2},
    {.name = "PWMB3", .gpio = BOARD_PWM_B3_GPIO, .ledc_channel = LEDC_CHANNEL_3},
    {.name = "PWMB4", .gpio = BOARD_PWM_B4_GPIO, .ledc_channel = LEDC_CHANNEL_4},
};

static servo_driver_config_t s_config;
static uint32_t s_duty_max = 0U;
static uint32_t s_period_us = SERVO_DRIVER_DEFAULT_PERIOD_US;
static bool s_initialized = false;

void servo_driver_default_config(servo_driver_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->speed_mode = LEDC_HIGH_SPEED_MODE;
    config->timer = LEDC_TIMER_1;
    config->duty_resolution = LEDC_TIMER_14_BIT;
    config->frequency_hz = SERVO_DRIVER_DEFAULT_FREQ_HZ;
    config->min_pulse_us = SERVO_DRIVER_DEFAULT_MIN_US;
    config->center_pulse_us = SERVO_DRIVER_DEFAULT_CENTER_US;
    config->max_pulse_us = SERVO_DRIVER_DEFAULT_MAX_US;
    config->channels = s_default_channels;
    config->channel_count = SERVO_DRIVER_MAX_CHANNELS;
}

uint32_t servo_driver_angle_to_pulse_us(uint16_t angle_deg)
{
    if (angle_deg > 180U) {
        angle_deg = 180U;
    }

    uint32_t span = s_config.max_pulse_us - s_config.min_pulse_us;
    return s_config.min_pulse_us + ((span * angle_deg) / 180U);
}

uint32_t servo_driver_pulse_us_to_duty(uint32_t pulse_us)
{
    return (pulse_us * s_duty_max) / s_period_us;
}

esp_err_t servo_driver_init(const servo_driver_config_t *config)
{
    servo_driver_config_t local_config;

    if (config == NULL) {
        servo_driver_default_config(&local_config);
        config = &local_config;
    }

    ESP_RETURN_ON_FALSE(config->channels != NULL, ESP_ERR_INVALID_ARG, TAG, "channels is null");
    ESP_RETURN_ON_FALSE(config->channel_count > 0U, ESP_ERR_INVALID_ARG, TAG, "channel count is zero");
    ESP_RETURN_ON_FALSE(config->channel_count <= SERVO_DRIVER_MAX_CHANNELS,
                        ESP_ERR_INVALID_ARG, TAG, "too many channels");
    ESP_RETURN_ON_FALSE(config->frequency_hz > 0U, ESP_ERR_INVALID_ARG, TAG, "bad frequency");
    ESP_RETURN_ON_FALSE(config->min_pulse_us < config->center_pulse_us,
                        ESP_ERR_INVALID_ARG, TAG, "bad min/center pulse");
    ESP_RETURN_ON_FALSE(config->center_pulse_us < config->max_pulse_us,
                        ESP_ERR_INVALID_ARG, TAG, "bad center/max pulse");

    memcpy(&s_config, config, sizeof(s_config));
    s_duty_max = (1UL << s_config.duty_resolution) - 1U;
    s_period_us = 1000000UL / s_config.frequency_hz;

    ledc_timer_config_t timer = {
        .speed_mode = s_config.speed_mode,
        .duty_resolution = s_config.duty_resolution,
        .timer_num = s_config.timer,
        .freq_hz = s_config.frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer config failed");

    uint32_t center_duty = servo_driver_pulse_us_to_duty(s_config.center_pulse_us);
    for (uint8_t i = 0U; i < s_config.channel_count; ++i) {
        ledc_channel_config_t channel = {
            .gpio_num = s_config.channels[i].gpio,
            .speed_mode = s_config.speed_mode,
            .channel = s_config.channels[i].ledc_channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = s_config.timer,
            .duty = center_duty,
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "channel config failed");
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t servo_driver_set_angle(uint8_t channel, uint16_t angle_deg)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");
    ESP_RETURN_ON_FALSE(channel < s_config.channel_count, ESP_ERR_INVALID_ARG, TAG, "bad channel");

    uint32_t pulse_us = servo_driver_angle_to_pulse_us(angle_deg);
    uint32_t duty = servo_driver_pulse_us_to_duty(pulse_us);

    ESP_RETURN_ON_ERROR(
        ledc_set_duty(s_config.speed_mode, s_config.channels[channel].ledc_channel, duty),
        TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(
        ledc_update_duty(s_config.speed_mode, s_config.channels[channel].ledc_channel),
        TAG, "update duty failed");

    ESP_LOGI(TAG, "ch%u %s GPIO%d angle=%u pulse=%" PRIu32 "us duty=%" PRIu32,
             channel, s_config.channels[channel].name, s_config.channels[channel].gpio,
             angle_deg, pulse_us, duty);
    return ESP_OK;
}

esp_err_t servo_driver_set_all_angle(uint16_t angle_deg)
{
    for (uint8_t i = 0U; i < s_config.channel_count; ++i) {
        ESP_RETURN_ON_ERROR(servo_driver_set_angle(i, angle_deg), TAG, "set angle failed");
    }
    return ESP_OK;
}
