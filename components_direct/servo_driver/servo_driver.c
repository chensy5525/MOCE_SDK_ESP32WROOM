#include "servo_driver.h"

#include <stddef.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "servo_driver";

static bool position_is_valid(ServoPosition position)
{
    switch (position) {
        case SERVO_POSITION_0_DEG:
        case SERVO_POSITION_45_DEG:
        case SERVO_POSITION_90_DEG:
        case SERVO_POSITION_135_DEG:
        case SERVO_POSITION_180_DEG:
            return true;
        default:
            return false;
    }
}

static esp_err_t position_to_pulse_width(const ServoDriverConfig *config,
                                         ServoPosition position,
                                         uint32_t *pulse_width_us)
{
    if ((config == NULL) || (pulse_width_us == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (position) {
        case SERVO_POSITION_0_DEG:
            *pulse_width_us = config->pulse_0_us;
            break;
        case SERVO_POSITION_45_DEG:
            *pulse_width_us = config->pulse_45_us;
            break;
        case SERVO_POSITION_90_DEG:
            *pulse_width_us = config->pulse_90_us;
            break;
        case SERVO_POSITION_135_DEG:
            *pulse_width_us = config->pulse_135_us;
            break;
        case SERVO_POSITION_180_DEG:
            *pulse_width_us = config->pulse_180_us;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t validate_config(const ServoDriverConfig *config)
{
    ESP_RETURN_ON_FALSE(config != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "config is null");
    ESP_RETURN_ON_FALSE(config->channel_count > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "channel count is zero");
    ESP_RETURN_ON_FALSE(config->channel_count <= SERVO_DRIVER_MAX_CHANNELS,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "too many channels");
    ESP_RETURN_ON_FALSE(config->frequency_hz == SERVO_DRIVER_PWM_FREQUENCY_HZ,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "frequency must be 50 Hz");
    ESP_RETURN_ON_FALSE((uint32_t)config->duty_resolution < 32U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "duty resolution is invalid");
    ESP_RETURN_ON_FALSE(config->pulse_0_us > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "0-degree pulse is zero");
    ESP_RETURN_ON_FALSE(config->pulse_0_us < config->pulse_45_us,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "0/45-degree pulses are invalid");
    ESP_RETURN_ON_FALSE(config->pulse_45_us < config->pulse_90_us,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "45/90-degree pulses are invalid");
    ESP_RETURN_ON_FALSE(config->pulse_90_us < config->pulse_135_us,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "90/135-degree pulses are invalid");
    ESP_RETURN_ON_FALSE(config->pulse_135_us < config->pulse_180_us,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "135/180-degree pulses are invalid");
    ESP_RETURN_ON_FALSE(position_is_valid(config->initial_position),
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "initial position is invalid");

    uint32_t period_us = 1000000U / config->frequency_hz;
    ESP_RETURN_ON_FALSE(config->pulse_180_us < period_us,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "pulse width exceeds PWM period");

    for (uint8_t index = 0U; index < config->channel_count; ++index) {
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(config->channels[index].gpio),
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "channel GPIO is not output-capable");

        for (uint8_t previous = 0U; previous < index; ++previous) {
            ESP_RETURN_ON_FALSE(
                config->channels[index].gpio != config->channels[previous].gpio,
                ESP_ERR_INVALID_ARG,
                TAG,
                "duplicate channel GPIO");
            ESP_RETURN_ON_FALSE(
                config->channels[index].ledc_channel !=
                    config->channels[previous].ledc_channel,
                ESP_ERR_INVALID_ARG,
                TAG,
                "duplicate LEDC channel");
        }
    }

    return ESP_OK;
}

static uint32_t pulse_width_to_duty(const ServoDriver *driver,
                                    uint32_t pulse_width_us)
{
    uint64_t scaled_duty = (uint64_t)pulse_width_us * driver->max_duty;
    return (uint32_t)((scaled_duty + (driver->pwm_period_us / 2U)) /
                      driver->pwm_period_us);
}

esp_err_t servo_driver_config_default(ServoDriverConfig *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *config = (ServoDriverConfig) {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .frequency_hz = SERVO_DRIVER_PWM_FREQUENCY_HZ,
        .pulse_0_us = SERVO_DRIVER_DEFAULT_0_PULSE_US,
        .pulse_45_us = SERVO_DRIVER_DEFAULT_45_PULSE_US,
        .pulse_90_us = SERVO_DRIVER_DEFAULT_90_PULSE_US,
        .pulse_135_us = SERVO_DRIVER_DEFAULT_135_PULSE_US,
        .pulse_180_us = SERVO_DRIVER_DEFAULT_180_PULSE_US,
        .initial_position = SERVO_POSITION_90_DEG,
        .channels = {{0}},
        .channel_count = 0U,
    };
    return ESP_OK;
}

esp_err_t servo_driver_init(ServoDriver *driver,
                            const ServoDriverConfig *config)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(validate_config(config), TAG, "invalid configuration");

    ServoDriver candidate = {0};
    candidate.config = *config;
    candidate.pwm_period_us = 1000000U / config->frequency_hz;
    candidate.max_duty = bsp_pwm_max_duty(config->duty_resolution);
    ESP_RETURN_ON_FALSE(candidate.max_duty > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "duty resolution is unsupported");

    uint32_t initial_pulse_us = 0U;
    ESP_RETURN_ON_ERROR(position_to_pulse_width(config,
                                                config->initial_position,
                                                &initial_pulse_us),
                        TAG,
                        "initial position is invalid");
    uint32_t initial_duty = pulse_width_to_duty(&candidate, initial_pulse_us);

    bsp_pwm_timer_config_t timer_config = {
        .speed_mode = config->speed_mode,
        .timer_num = config->timer,
        .duty_resolution = config->duty_resolution,
        .frequency_hz = config->frequency_hz,
    };
    ESP_RETURN_ON_ERROR(bsp_pwm_timer_init(&timer_config),
                        TAG,
                        "PWM timer initialization failed");

    uint8_t initialized_channels = 0U;
    for (uint8_t index = 0U; index < config->channel_count; ++index) {
        bsp_pwm_channel_config_t channel_config = {
            .gpio_num = config->channels[index].gpio,
            .speed_mode = config->speed_mode,
            .channel = config->channels[index].ledc_channel,
            .timer_num = config->timer,
            .duty = initial_duty,
        };
        esp_err_t error = bsp_pwm_channel_init(&channel_config);
        if (error != ESP_OK) {
            for (uint8_t rollback = 0U;
                 rollback < initialized_channels;
                 ++rollback) {
                (void)bsp_pwm_stop(config->speed_mode,
                                   config->channels[rollback].ledc_channel,
                                   0U);
            }
            (void)bsp_pwm_timer_deinit(config->speed_mode, config->timer);
            ESP_LOGE(TAG,
                     "PWM channel %u initialization failed: %s",
                     (unsigned)index,
                     esp_err_to_name(error));
            return error;
        }

        candidate.commanded_positions[index] = config->initial_position;
        ++initialized_channels;
    }

    candidate.initialized = true;
    *driver = candidate;
    return ESP_OK;
}

esp_err_t servo_driver_set_position(ServoDriver *driver,
                                    uint8_t channel_index,
                                    ServoPosition position)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index >= driver->config.channel_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t pulse_width_us = 0U;
    ESP_RETURN_ON_ERROR(position_to_pulse_width(&driver->config,
                                                position,
                                                &pulse_width_us),
                        TAG,
                        "position is invalid");
    uint32_t duty = pulse_width_to_duty(driver, pulse_width_us);
    esp_err_t error = bsp_pwm_set_duty(
        driver->config.speed_mode,
        driver->config.channels[channel_index].ledc_channel,
        duty);
    if (error != ESP_OK) {
        return error;
    }

    driver->commanded_positions[channel_index] = position;
    return ESP_OK;
}

esp_err_t servo_driver_set_all_positions(ServoDriver *driver,
                                         ServoPosition position)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (uint8_t index = 0U; index < driver->config.channel_count; ++index) {
        esp_err_t error = servo_driver_set_position(driver, index, position);
        if (error != ESP_OK) {
            return error;
        }
    }
    return ESP_OK;
}

esp_err_t servo_driver_get_commanded_position(const ServoDriver *driver,
                                              uint8_t channel_index,
                                              ServoPosition *position)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((position == NULL) || (channel_index >= driver->config.channel_count)) {
        return ESP_ERR_INVALID_ARG;
    }

    *position = driver->commanded_positions[channel_index];
    return ESP_OK;
}

esp_err_t servo_driver_deinit(ServoDriver *driver)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t first_error = ESP_OK;
    for (uint8_t index = 0U; index < driver->config.channel_count; ++index) {
        esp_err_t error = bsp_pwm_stop(
            driver->config.speed_mode,
            driver->config.channels[index].ledc_channel,
            0U);
        if ((first_error == ESP_OK) && (error != ESP_OK)) {
            first_error = error;
        }
    }

    esp_err_t timer_error = bsp_pwm_timer_deinit(driver->config.speed_mode,
                                                 driver->config.timer);
    if ((first_error == ESP_OK) && (timer_error != ESP_OK)) {
        first_error = timer_error;
    }

    *driver = (ServoDriver) {0};
    return first_error;
}

esp_err_t servo_driver_position_to_degrees(ServoPosition position,
                                           uint16_t *degrees)
{
    if (degrees == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (position) {
        case SERVO_POSITION_0_DEG:
            *degrees = 0U;
            break;
        case SERVO_POSITION_45_DEG:
            *degrees = 45U;
            break;
        case SERVO_POSITION_90_DEG:
            *degrees = 90U;
            break;
        case SERVO_POSITION_135_DEG:
            *degrees = 135U;
            break;
        case SERVO_POSITION_180_DEG:
            *degrees = 180U;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
