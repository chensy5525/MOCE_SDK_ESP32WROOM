#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_pwm.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVO_DRIVER_MAX_CHANNELS          4U
#define SERVO_DRIVER_PWM_FREQUENCY_HZ      50U
#define SERVO_DRIVER_DEFAULT_0_PULSE_US    1000U
#define SERVO_DRIVER_DEFAULT_45_PULSE_US   1250U
#define SERVO_DRIVER_DEFAULT_90_PULSE_US   1500U
#define SERVO_DRIVER_DEFAULT_135_PULSE_US  1750U
#define SERVO_DRIVER_DEFAULT_180_PULSE_US  2000U
#define SERVO_DRIVER_MIN_PULSE_US          1000U
#define SERVO_DRIVER_MAX_PULSE_US          2000U
#define SERVO_DRIVER_INITIALIZER            {0}

typedef enum {
    SERVO_POSITION_0_DEG = 0,
    SERVO_POSITION_45_DEG,
    SERVO_POSITION_90_DEG,
    SERVO_POSITION_135_DEG,
    SERVO_POSITION_180_DEG,
} ServoPosition;

typedef struct {
    gpio_num_t gpio;
    ledc_channel_t ledc_channel;
} ServoChannelConfig;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer;
    ledc_timer_bit_t duty_resolution;
    uint32_t frequency_hz;
    uint32_t pulse_0_us;
    uint32_t pulse_45_us;
    uint32_t pulse_90_us;
    uint32_t pulse_135_us;
    uint32_t pulse_180_us;
    ServoPosition initial_position;
    ServoChannelConfig channels[SERVO_DRIVER_MAX_CHANNELS];
    uint8_t channel_count;
} ServoDriverConfig;

typedef struct {
    ServoDriverConfig config;
    ServoPosition commanded_positions[SERVO_DRIVER_MAX_CHANNELS];
    uint32_t pwm_period_us;
    uint32_t max_duty;
    bool initialized;
    bool cleanup_required;
} ServoDriver;

/** Populate conservative MG90S nominal defaults with no resource bindings. */
esp_err_t servo_driver_config_default(ServoDriverConfig *config);

/**
 * Initialize a zero-initialized driver handle and all configured PWM channels.
 * The caller retains ownership of the GPIO/timer/channel resource assignment.
 */
esp_err_t servo_driver_init(ServoDriver *driver,
                            const ServoDriverConfig *config);

/** Apply one of the five supported nominal position commands. */
esp_err_t servo_driver_set_position(ServoDriver *driver,
                                    uint8_t channel_index,
                                    ServoPosition position);

/**
 * Apply a position sequentially to every channel.
 * If any update fails, restore every channel to initial_position. If that
 * recovery also fails, stop all configured PWM outputs; an incomplete shutdown
 * leaves the handle in cleanup-required state.
 */
esp_err_t servo_driver_set_all_positions(ServoDriver *driver,
                                         ServoPosition position);

/** Return software command state only; this is not physical position feedback. */
esp_err_t servo_driver_get_commanded_position(const ServoDriver *driver,
                                              uint8_t channel_index,
                                              ServoPosition *position);

/**
 * Stop configured outputs and release the PWM timer.
 * On failure the handle rejects commands and remains valid for a cleanup retry.
 */
esp_err_t servo_driver_deinit(ServoDriver *driver);

/** Convert a supported position token to its nominal degree label. */
esp_err_t servo_driver_position_to_degrees(ServoPosition position,
                                           uint16_t *degrees);

#ifdef __cplusplus
}
#endif
