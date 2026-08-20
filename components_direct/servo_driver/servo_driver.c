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

static bool angle_is_valid(uint16_t angle_degrees)
{
    return angle_degrees <= SERVO_DRIVER_MAX_ANGLE_DEG;
}

static esp_err_t angle_to_pulse_width(const ServoDriverConfig *config,
                                      uint16_t angle_degrees,
                                      uint32_t *pulse_width_us)
{
    if ((config == NULL) || (pulse_width_us == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!angle_is_valid(angle_degrees)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t lower_angle = 0U;
    uint16_t upper_angle = 45U;
    uint32_t lower_pulse_us = config->pulse_0_us;
    uint32_t upper_pulse_us = config->pulse_45_us;

    if (angle_degrees > 135U) {
        lower_angle = 135U;
        upper_angle = 180U;
        lower_pulse_us = config->pulse_135_us;
        upper_pulse_us = config->pulse_180_us;
    } else if (angle_degrees > 90U) {
        lower_angle = 90U;
        upper_angle = 135U;
        lower_pulse_us = config->pulse_90_us;
        upper_pulse_us = config->pulse_135_us;
    } else if (angle_degrees > 45U) {
        lower_angle = 45U;
        upper_angle = 90U;
        lower_pulse_us = config->pulse_45_us;
        upper_pulse_us = config->pulse_90_us;
    }

    uint32_t pulse_span_us = upper_pulse_us - lower_pulse_us;
    uint16_t angle_span = upper_angle - lower_angle;
    uint16_t angle_offset = angle_degrees - lower_angle;
    uint64_t scaled_offset = (uint64_t)pulse_span_us * angle_offset;
    *pulse_width_us = lower_pulse_us +
                      (uint32_t)((scaled_offset + (angle_span / 2U)) /
                                 angle_span);
    return ESP_OK;
}

static esp_err_t position_to_degrees(ServoPosition position,
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

static esp_err_t degrees_to_position(uint16_t degrees,
                                     ServoPosition *position)
{
    if (position == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (degrees) {
        case 0U:
            *position = SERVO_POSITION_0_DEG;
            break;
        case 45U:
            *position = SERVO_POSITION_45_DEG;
            break;
        case 90U:
            *position = SERVO_POSITION_90_DEG;
            break;
        case 135U:
            *position = SERVO_POSITION_135_DEG;
            break;
        case 180U:
            *position = SERVO_POSITION_180_DEG;
            break;
        default:
            return ESP_ERR_INVALID_STATE;
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
    ESP_RETURN_ON_FALSE((uint32_t)config->speed_mode <
                            (uint32_t)LEDC_SPEED_MODE_MAX,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "LEDC speed mode is invalid");
    ESP_RETURN_ON_FALSE((uint32_t)config->timer <
                            (uint32_t)LEDC_TIMER_MAX,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "LEDC timer is invalid");
    ESP_RETURN_ON_FALSE(bsp_pwm_max_duty(config->duty_resolution) > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "duty resolution is invalid");
    ESP_RETURN_ON_FALSE(config->pulse_0_us >= SERVO_DRIVER_MIN_PULSE_US,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "minimum pulse is outside the supported envelope");
    ESP_RETURN_ON_FALSE(config->pulse_180_us <= SERVO_DRIVER_MAX_PULSE_US,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "maximum pulse is outside the supported envelope");
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
        ESP_RETURN_ON_FALSE(
            (uint32_t)config->channels[index].ledc_channel <
                (uint32_t)LEDC_CHANNEL_MAX,
            ESP_ERR_INVALID_ARG,
            TAG,
            "LEDC channel is invalid");

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

static esp_err_t release_pwm_resources(const ServoDriverConfig *config,
                                       uint8_t channel_count)
{
    esp_err_t first_error = ESP_OK;

    for (uint8_t index = 0U; index < channel_count; ++index) {
        esp_err_t error = bsp_pwm_stop(config->speed_mode,
                                       config->channels[index].ledc_channel,
                                       0U);
        if ((first_error == ESP_OK) && (error != ESP_OK)) {
            first_error = error;
        }
    }

    esp_err_t timer_error = bsp_pwm_timer_deinit(config->speed_mode,
                                                 config->timer);
    if ((first_error == ESP_OK) && (timer_error != ESP_OK)) {
        first_error = timer_error;
    }

    return first_error;
}

static uint32_t pulse_width_to_duty(const ServoDriver *driver,
                                    uint32_t pulse_width_us)
{
    uint64_t scaled_duty = (uint64_t)pulse_width_us * driver->max_duty;
    return (uint32_t)((scaled_duty + (driver->pwm_period_us / 2U)) /
                      driver->pwm_period_us);
}

static esp_err_t apply_angle_to_all_channels(ServoDriver *driver,
                                             uint16_t angle_degrees)
{
    for (uint8_t index = 0U; index < driver->config.channel_count; ++index) {
        esp_err_t error = servo_driver_set_angle(driver, index, angle_degrees);
        if (error != ESP_OK) {
            return error;
        }
    }
    return ESP_OK;
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
    if (driver->initialized || driver->cleanup_required) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(validate_config(config), TAG, "invalid configuration");

    ServoDriver candidate = {0};
    candidate.config = *config;
    candidate.pwm_period_us = 1000000U / config->frequency_hz;
    candidate.max_duty = bsp_pwm_max_duty(config->duty_resolution);

    uint16_t initial_angle_degrees = 0U;
    ESP_RETURN_ON_ERROR(position_to_degrees(config->initial_position,
                                            &initial_angle_degrees),
                        TAG,
                        "initial position is invalid");
    uint32_t initial_pulse_us = 0U;
    ESP_RETURN_ON_ERROR(angle_to_pulse_width(config,
                                             initial_angle_degrees,
                                             &initial_pulse_us),
                        TAG,
                        "initial angle conversion failed");
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
            esp_err_t rollback_error = release_pwm_resources(
                config,
                config->channel_count);
            if (rollback_error != ESP_OK) {
                candidate.initialized = false;
                candidate.cleanup_required = true;
                *driver = candidate;
                ESP_LOGE(TAG,
                         "PWM initialization rollback incomplete: %s",
                         esp_err_to_name(rollback_error));
            }
            ESP_LOGE(TAG,
                     "PWM channel %u initialization failed: %s",
                     (unsigned)index,
                     esp_err_to_name(error));
            return error;
        }

        candidate.commanded_angles_deg[index] = initial_angle_degrees;
    }

    candidate.initialized = true;
    *driver = candidate;
    return ESP_OK;
}

esp_err_t servo_driver_set_angle(ServoDriver *driver,
                                 uint8_t channel_index,
                                 uint16_t angle_degrees)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index >= driver->config.channel_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t pulse_width_us = 0U;
    ESP_RETURN_ON_ERROR(angle_to_pulse_width(&driver->config,
                                             angle_degrees,
                                             &pulse_width_us),
                        TAG,
                        "angle is invalid");
    uint32_t duty = pulse_width_to_duty(driver, pulse_width_us);
    esp_err_t error = bsp_pwm_set_duty(
        driver->config.speed_mode,
        driver->config.channels[channel_index].ledc_channel,
        duty);
    if (error != ESP_OK) {
        return error;
    }

    driver->commanded_angles_deg[channel_index] = angle_degrees;
    return ESP_OK;
}

esp_err_t servo_driver_set_all_angles(ServoDriver *driver,
                                      uint16_t angle_degrees)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!angle_is_valid(angle_degrees)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t command_error = apply_angle_to_all_channels(driver, angle_degrees);
    if (command_error == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGE(TAG,
             "coordinated command failed: %s; restoring all channels to the initial position",
             esp_err_to_name(command_error));
    uint16_t safe_angle_degrees = 0U;
    esp_err_t safe_error = position_to_degrees(driver->config.initial_position,
                                               &safe_angle_degrees);
    if (safe_error == ESP_OK) {
        safe_error = apply_angle_to_all_channels(driver, safe_angle_degrees);
    }
    if (safe_error == ESP_OK) {
        ESP_LOGW(TAG, "all channels restored to the initial position");
        return command_error;
    }

    ESP_LOGE(TAG,
             "safe-position recovery failed: %s; stopping all PWM outputs",
             esp_err_to_name(safe_error));
    esp_err_t shutdown_error = servo_driver_deinit(driver);
    if (shutdown_error != ESP_OK) {
        ESP_LOGE(TAG,
                 "PWM shutdown incomplete: %s; cleanup retry required",
                 esp_err_to_name(shutdown_error));
    }
    return command_error;
}

esp_err_t servo_driver_get_commanded_angle(const ServoDriver *driver,
                                           uint8_t channel_index,
                                           uint16_t *angle_degrees)
{
    if ((driver == NULL) || (angle_degrees == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index >= driver->config.channel_count) {
        return ESP_ERR_INVALID_ARG;
    }

    *angle_degrees = driver->commanded_angles_deg[channel_index];
    return ESP_OK;
}

esp_err_t servo_driver_set_position(ServoDriver *driver,
                                    uint8_t channel_index,
                                    ServoPosition position)
{
    uint16_t angle_degrees = 0U;
    esp_err_t error = position_to_degrees(position, &angle_degrees);
    return (error == ESP_OK) ?
           servo_driver_set_angle(driver, channel_index, angle_degrees) : error;
}

esp_err_t servo_driver_set_all_positions(ServoDriver *driver,
                                         ServoPosition position)
{
    uint16_t angle_degrees = 0U;
    esp_err_t error = position_to_degrees(position, &angle_degrees);
    return (error == ESP_OK) ?
           servo_driver_set_all_angles(driver, angle_degrees) : error;
}

esp_err_t servo_driver_get_commanded_position(const ServoDriver *driver,
                                              uint8_t channel_index,
                                              ServoPosition *position)
{
    if ((driver == NULL) || (position == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel_index >= driver->config.channel_count) {
        return ESP_ERR_INVALID_ARG;
    }

    return degrees_to_position(driver->commanded_angles_deg[channel_index],
                               position);
}

esp_err_t servo_driver_deinit(ServoDriver *driver)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!driver->initialized && !driver->cleanup_required) {
        return ESP_ERR_INVALID_STATE;
    }

    driver->initialized = false;
    driver->cleanup_required = true;

    esp_err_t error = release_pwm_resources(&driver->config,
                                            driver->config.channel_count);
    if (error != ESP_OK) {
        return error;
    }

    *driver = (ServoDriver) {0};
    return ESP_OK;
}

esp_err_t servo_driver_position_to_degrees(ServoPosition position,
                                           uint16_t *degrees)
{
    return position_to_degrees(position, degrees);
}
