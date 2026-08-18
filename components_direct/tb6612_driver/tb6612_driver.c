#include "tb6612_driver.h"

#include <stddef.h>

#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "esp_check.h"
#include "esp_log.h"

#define TB6612_DEFAULT_PWM_FREQUENCY_HZ  10000U
#define TB6612_DEFAULT_PWM_RESOLUTION    LEDC_TIMER_12_BIT
#define TB6612_GPIO_LEVEL_LOW            0U
#define TB6612_GPIO_LEVEL_HIGH           1U

static const char *TAG = "tb6612_driver";

static bool motor_is_valid(Tb6612Motor motor)
{
    switch (motor) {
        case TB6612_MOTOR_A:
        case TB6612_MOTOR_B:
            return true;
        default:
            return false;
    }
}

static bool direction_is_valid(Tb6612Direction direction)
{
    switch (direction) {
        case TB6612_DIRECTION_STOP:
        case TB6612_DIRECTION_FORWARD:
        case TB6612_DIRECTION_REVERSE:
            return true;
        default:
            return false;
    }
}

static esp_err_t configure_output_gpio(gpio_num_t gpio)
{
    const bsp_gpio_config_t gpio_config = {
        .pin = gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up = false,
        .pull_down = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return bsp_gpio_config(&gpio_config);
}

static void remember_first_error(esp_err_t *first_error, esp_err_t error)
{
    if ((*first_error == ESP_OK) && (error != ESP_OK)) {
        *first_error = error;
    }
}

static esp_err_t configure_output_low(gpio_num_t gpio)
{
    esp_err_t error = configure_output_gpio(gpio);
    if (error != ESP_OK) {
        return error;
    }
    return bsp_gpio_set_level(gpio, TB6612_GPIO_LEVEL_LOW);
}

static uint32_t percent_to_duty(uint8_t percent, uint32_t max_duty)
{
    return (uint32_t)(((uint64_t)max_duty * percent + 50U) / 100U);
}

static uint32_t stby_level(const Tb6612DriverConfig *config, bool active)
{
    bool high = active == config->stby_active_high;
    return high ? TB6612_GPIO_LEVEL_HIGH : TB6612_GPIO_LEVEL_LOW;
}

static esp_err_t set_direction_pins(const Tb6612DriverConfig *config,
                                    Tb6612Motor motor,
                                    Tb6612Direction direction)
{
    uint32_t in1_level = TB6612_GPIO_LEVEL_LOW;
    uint32_t in2_level = TB6612_GPIO_LEVEL_LOW;

    switch (direction) {
        case TB6612_DIRECTION_STOP:
            break;
        case TB6612_DIRECTION_FORWARD:
            in1_level = TB6612_GPIO_LEVEL_HIGH;
            break;
        case TB6612_DIRECTION_REVERSE:
            in2_level = TB6612_GPIO_LEVEL_HIGH;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    const Tb6612ChannelConfig *channel = &config->channels[(uint8_t)motor];
    esp_err_t error = bsp_gpio_set_level(channel->in1_gpio, in1_level);
    if (error != ESP_OK) {
        return error;
    }
    return bsp_gpio_set_level(channel->in2_gpio, in2_level);
}

static esp_err_t release_bound_gpios(const Tb6612DriverConfig *config)
{
    esp_err_t first_error = ESP_OK;
    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        remember_first_error(
            &first_error,
            bsp_gpio_reset(config->channels[index].pwm_gpio));
        remember_first_error(
            &first_error,
            bsp_gpio_reset(config->channels[index].in1_gpio));
        remember_first_error(
            &first_error,
            bsp_gpio_reset(config->channels[index].in2_gpio));
    }
    if (config->control_stby) {
        remember_first_error(&first_error, bsp_gpio_reset(config->stby_gpio));
    }
    return first_error;
}

static esp_err_t hold_motor_pins_safe(const Tb6612DriverConfig *config)
{
    esp_err_t first_error = ESP_OK;
    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        remember_first_error(
            &first_error,
            configure_output_low(config->channels[index].pwm_gpio));
        remember_first_error(
            &first_error,
            configure_output_low(config->channels[index].in1_gpio));
        remember_first_error(
            &first_error,
            configure_output_low(config->channels[index].in2_gpio));
    }
    return first_error;
}

static esp_err_t cleanup_initialized_resources(
    const Tb6612DriverConfig *config,
    uint8_t initialized_channels,
    bool timer_initialized,
    bool stby_configured)
{
    esp_err_t first_error = ESP_OK;
    bool stby_inactive = false;

    for (uint8_t index = 0U; index < initialized_channels; ++index) {
        remember_first_error(
            &first_error,
            bsp_pwm_stop(config->speed_mode,
                         config->channels[index].pwm_channel,
                         TB6612_GPIO_LEVEL_LOW));
    }
    if (timer_initialized) {
        remember_first_error(
            &first_error,
            bsp_pwm_timer_deinit(config->speed_mode, config->timer));
    }

    if (config->control_stby && stby_configured) {
        esp_err_t error = bsp_gpio_set_level(config->stby_gpio,
                                              stby_level(config, false));
        remember_first_error(&first_error, error);
        stby_inactive = error == ESP_OK;
    }

    if (config->control_stby && stby_inactive) {
        remember_first_error(&first_error, release_bound_gpios(config));
    } else {
        remember_first_error(&first_error, hold_motor_pins_safe(config));
    }
    return first_error;
}

static void log_cleanup_failure(esp_err_t cleanup_error)
{
    if (cleanup_error != ESP_OK) {
        ESP_LOGE(TAG,
                 "cleanup could not prove the safe state: %s",
                 esp_err_to_name(cleanup_error));
    }
}

static esp_err_t validate_config(const Tb6612DriverConfig *config)
{
    ESP_RETURN_ON_FALSE(config != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "config is null");
    ESP_RETURN_ON_FALSE(config->frequency_hz > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "PWM frequency is zero");
    ESP_RETURN_ON_FALSE(
        (uint32_t)config->speed_mode < (uint32_t)LEDC_SPEED_MODE_MAX,
        ESP_ERR_INVALID_ARG,
        TAG,
        "PWM speed mode is invalid");
    ESP_RETURN_ON_FALSE(
        (uint32_t)config->timer < (uint32_t)LEDC_TIMER_MAX,
        ESP_ERR_INVALID_ARG,
        TAG,
        "PWM timer is invalid");
    ESP_RETURN_ON_FALSE(bsp_pwm_max_duty(config->duty_resolution) > 0U,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "PWM resolution is invalid");

    gpio_num_t pins[(TB6612_DRIVER_MOTOR_COUNT * 3U) + 1U] = {GPIO_NUM_NC};
    uint8_t pin_count = 0U;
    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        const Tb6612ChannelConfig *channel = &config->channels[index];
        ESP_RETURN_ON_FALSE(
            (uint32_t)channel->pwm_channel < (uint32_t)LEDC_CHANNEL_MAX,
            ESP_ERR_INVALID_ARG,
            TAG,
            "PWM channel is invalid");
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(channel->pwm_gpio),
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "PWM GPIO is not output-capable");
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(channel->in1_gpio),
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "IN1 GPIO is not output-capable");
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(channel->in2_gpio),
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "IN2 GPIO is not output-capable");
        pins[pin_count++] = channel->pwm_gpio;
        pins[pin_count++] = channel->in1_gpio;
        pins[pin_count++] = channel->in2_gpio;
    }

    ESP_RETURN_ON_FALSE(
        config->channels[TB6612_MOTOR_A].pwm_channel !=
            config->channels[TB6612_MOTOR_B].pwm_channel,
        ESP_ERR_INVALID_ARG,
        TAG,
        "duplicate PWM channel");

    if (config->control_stby) {
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(config->stby_gpio),
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "STBY GPIO is not output-capable");
        pins[pin_count++] = config->stby_gpio;
    }

    for (uint8_t index = 0U; index < pin_count; ++index) {
        for (uint8_t previous = 0U; previous < index; ++previous) {
            ESP_RETURN_ON_FALSE(pins[index] != pins[previous],
                                ESP_ERR_INVALID_ARG,
                                TAG,
                                "duplicate GPIO binding");
        }
    }
    return ESP_OK;
}

esp_err_t tb6612_driver_config_default(Tb6612DriverConfig *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *config = (Tb6612DriverConfig) {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer = LEDC_TIMER_0,
        .duty_resolution = TB6612_DEFAULT_PWM_RESOLUTION,
        .frequency_hz = TB6612_DEFAULT_PWM_FREQUENCY_HZ,
        .channels = {
            {
                .pwm_gpio = GPIO_NUM_NC,
                .in1_gpio = GPIO_NUM_NC,
                .in2_gpio = GPIO_NUM_NC,
                .pwm_channel = LEDC_CHANNEL_0,
            },
            {
                .pwm_gpio = GPIO_NUM_NC,
                .in1_gpio = GPIO_NUM_NC,
                .in2_gpio = GPIO_NUM_NC,
                .pwm_channel = LEDC_CHANNEL_1,
            },
        },
        .control_stby = false,
        .stby_gpio = GPIO_NUM_NC,
        .stby_active_high = true,
    };
    return ESP_OK;
}

esp_err_t tb6612_driver_init(Tb6612Driver *driver,
                             const Tb6612DriverConfig *config)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(validate_config(config), TAG, "invalid configuration");

    Tb6612Driver candidate = TB6612_DRIVER_INITIALIZER;
    candidate.config = *config;
    candidate.max_duty = bsp_pwm_max_duty(config->duty_resolution);
    bool stby_configured = false;
    bool timer_initialized = false;
    uint8_t initialized_channels = 0U;

    if (config->control_stby) {
        esp_err_t error = configure_output_gpio(config->stby_gpio);
        if (error != ESP_OK) {
            return error;
        }
        stby_configured = true;
        error = bsp_gpio_set_level(config->stby_gpio,
                                   stby_level(config, false));
        if (error != ESP_OK) {
            log_cleanup_failure(cleanup_initialized_resources(
                config, initialized_channels, timer_initialized, stby_configured));
            return error;
        }
    }

    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        esp_err_t error = configure_output_gpio(config->channels[index].in1_gpio);
        if (error == ESP_OK) {
            error = configure_output_gpio(config->channels[index].in2_gpio);
        }
        if (error == ESP_OK) {
            error = set_direction_pins(config,
                                       (Tb6612Motor)index,
                                       TB6612_DIRECTION_STOP);
        }
        if (error != ESP_OK) {
            log_cleanup_failure(cleanup_initialized_resources(
                config, initialized_channels, timer_initialized, stby_configured));
            return error;
        }
    }

    const bsp_pwm_timer_config_t timer_config = {
        .speed_mode = config->speed_mode,
        .timer_num = config->timer,
        .duty_resolution = config->duty_resolution,
        .frequency_hz = config->frequency_hz,
    };
    esp_err_t error = bsp_pwm_timer_init(&timer_config);
    if (error != ESP_OK) {
        log_cleanup_failure(cleanup_initialized_resources(
            config, initialized_channels, timer_initialized, stby_configured));
        return error;
    }
    timer_initialized = true;

    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        const bsp_pwm_channel_config_t channel_config = {
            .gpio_num = config->channels[index].pwm_gpio,
            .speed_mode = config->speed_mode,
            .channel = config->channels[index].pwm_channel,
            .timer_num = config->timer,
            .duty = 0U,
        };
        error = bsp_pwm_channel_init(&channel_config);
        if (error != ESP_OK) {
            log_cleanup_failure(cleanup_initialized_resources(
                config, initialized_channels, timer_initialized, stby_configured));
            return error;
        }
        candidate.commanded_directions[index] = TB6612_DIRECTION_STOP;
        candidate.commanded_duty_percent[index] = 0U;
        ++initialized_channels;
    }

    if (config->control_stby) {
        error = bsp_gpio_set_level(config->stby_gpio,
                                   stby_level(config, true));
        if (error != ESP_OK) {
            log_cleanup_failure(cleanup_initialized_resources(
                config, initialized_channels, timer_initialized, stby_configured));
            return error;
        }
    }

    candidate.initialized = true;
    *driver = candidate;
    return ESP_OK;
}

esp_err_t tb6612_driver_set_output(Tb6612Driver *driver,
                                   Tb6612Motor motor,
                                   Tb6612Direction direction,
                                   uint8_t duty_percent)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!motor_is_valid(motor) || !direction_is_valid(direction) ||
        (duty_percent > TB6612_DRIVER_MAX_DUTY_PERCENT) ||
        ((direction == TB6612_DIRECTION_STOP) && (duty_percent != 0U))) {
        return ESP_ERR_INVALID_ARG;
    }

    const Tb6612ChannelConfig *channel =
        &driver->config.channels[(uint8_t)motor];
    esp_err_t error = bsp_pwm_set_duty(driver->config.speed_mode,
                                       channel->pwm_channel,
                                       0U);
    if (error != ESP_OK) {
        return error;
    }
    driver->commanded_directions[(uint8_t)motor] = TB6612_DIRECTION_STOP;
    driver->commanded_duty_percent[(uint8_t)motor] = 0U;

    error = set_direction_pins(&driver->config, motor, direction);
    if (error != ESP_OK) {
        esp_err_t safe_error = set_direction_pins(&driver->config,
                                                   motor,
                                                   TB6612_DIRECTION_STOP);
        log_cleanup_failure(safe_error);
        return error;
    }

    uint32_t duty = percent_to_duty(duty_percent, driver->max_duty);
    error = bsp_pwm_set_duty(driver->config.speed_mode,
                             channel->pwm_channel,
                             duty);
    if (error != ESP_OK) {
        esp_err_t safe_error = set_direction_pins(&driver->config,
                                                   motor,
                                                   TB6612_DIRECTION_STOP);
        log_cleanup_failure(safe_error);
        return error;
    }

    driver->commanded_directions[(uint8_t)motor] = direction;
    driver->commanded_duty_percent[(uint8_t)motor] = duty_percent;
    return ESP_OK;
}

esp_err_t tb6612_driver_stop(Tb6612Driver *driver, Tb6612Motor motor)
{
    return tb6612_driver_set_output(driver,
                                    motor,
                                    TB6612_DIRECTION_STOP,
                                    0U);
}

esp_err_t tb6612_driver_stop_all(Tb6612Driver *driver)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t first_error = ESP_OK;
    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        esp_err_t error = tb6612_driver_stop(driver, (Tb6612Motor)index);
        remember_first_error(&first_error, error);
    }
    return first_error;
}

esp_err_t tb6612_driver_get_commanded_output(
    const Tb6612Driver *driver,
    Tb6612Motor motor,
    Tb6612Direction *direction,
    uint8_t *duty_percent)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!motor_is_valid(motor) || (direction == NULL) ||
        (duty_percent == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    *direction = driver->commanded_directions[(uint8_t)motor];
    *duty_percent = driver->commanded_duty_percent[(uint8_t)motor];
    return ESP_OK;
}

esp_err_t tb6612_driver_deinit(Tb6612Driver *driver)
{
    if ((driver == NULL) || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t first_error = tb6612_driver_stop_all(driver);
    bool stby_inactive = false;
    if (driver->config.control_stby) {
        esp_err_t error = bsp_gpio_set_level(
            driver->config.stby_gpio,
            stby_level(&driver->config, false));
        remember_first_error(&first_error, error);
        stby_inactive = error == ESP_OK;
    }

    for (uint8_t index = 0U; index < TB6612_DRIVER_MOTOR_COUNT; ++index) {
        esp_err_t error = bsp_pwm_stop(
            driver->config.speed_mode,
            driver->config.channels[index].pwm_channel,
            0U);
        remember_first_error(&first_error, error);
    }

    esp_err_t error = bsp_pwm_timer_deinit(driver->config.speed_mode,
                                           driver->config.timer);
    remember_first_error(&first_error, error);

    if (driver->config.control_stby && stby_inactive) {
        remember_first_error(&first_error,
                             release_bound_gpios(&driver->config));
    } else {
        remember_first_error(&first_error,
                             hold_motor_pins_safe(&driver->config));
    }
    *driver = (Tb6612Driver)TB6612_DRIVER_INITIALIZER;
    return first_error;
}
