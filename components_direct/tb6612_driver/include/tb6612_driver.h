#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TB6612_DRIVER_MOTOR_COUNT       2U
#define TB6612_DRIVER_MAX_DUTY_PERCENT  100U
#define TB6612_DRIVER_INITIALIZER       {0}

typedef enum {
    TB6612_MOTOR_A = 0,
    TB6612_MOTOR_B,
} Tb6612Motor;

typedef enum {
    TB6612_DIRECTION_STOP = 0,
    TB6612_DIRECTION_FORWARD,
    TB6612_DIRECTION_REVERSE,
} Tb6612Direction;

typedef struct {
    gpio_num_t pwm_gpio;
    gpio_num_t in1_gpio;
    gpio_num_t in2_gpio;
    ledc_channel_t pwm_channel;
} Tb6612ChannelConfig;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer;
    ledc_timer_bit_t duty_resolution;
    uint32_t frequency_hz;
    Tb6612ChannelConfig channels[TB6612_DRIVER_MOTOR_COUNT];
    bool control_stby;
    gpio_num_t stby_gpio;
    bool stby_active_high;
} Tb6612DriverConfig;

typedef struct {
    Tb6612DriverConfig config;
    Tb6612Direction commanded_directions[TB6612_DRIVER_MOTOR_COUNT];
    uint8_t commanded_duty_percent[TB6612_DRIVER_MOTOR_COUNT];
    uint32_t max_duty;
    bool initialized;
} Tb6612Driver;

/**
 * Populate transport defaults without assigning board GPIO resources.
 * STBY control defaults to required, so initialization fails until the caller
 * binds a valid STBY GPIO or explicitly selects externally enabled hardware.
 */
esp_err_t tb6612_driver_config_default(Tb6612DriverConfig *config);

/**
 * Initialize a TB6612 handle in the stopped state.
 * The caller must first initialize the handle with TB6612_DRIVER_INITIALIZER.
 * Setting control_stby=false is valid only when the schematic proves that STBY
 * is held active independently of firmware.
 */
esp_err_t tb6612_driver_init(Tb6612Driver *driver,
                             const Tb6612DriverConfig *config);

/**
 * Apply direction and duty to one motor.
 * STOP accepts only zero duty; direction changes pass through zero PWM.
 */
esp_err_t tb6612_driver_set_output(Tb6612Driver *driver,
                                   Tb6612Motor motor,
                                   Tb6612Direction direction,
                                   uint8_t duty_percent);

esp_err_t tb6612_driver_stop(Tb6612Driver *driver, Tb6612Motor motor);
esp_err_t tb6612_driver_stop_all(Tb6612Driver *driver);

/** Return the last fully accepted software command, not motor feedback. */
esp_err_t tb6612_driver_get_commanded_output(
    const Tb6612Driver *driver,
    Tb6612Motor motor,
    Tb6612Direction *direction,
    uint8_t *duty_percent);

/**
 * Stop both outputs and disable controlled STBY.
 * Pins are released only after controlled STBY is proven inactive. When STBY
 * is external, all six motor-control pins remain GPIO outputs driven low.
 */
esp_err_t tb6612_driver_deinit(Tb6612Driver *driver);

#ifdef __cplusplus
}
#endif
