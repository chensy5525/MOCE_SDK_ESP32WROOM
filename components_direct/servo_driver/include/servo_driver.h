#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVO_DRIVER_MAX_CHANNELS       4U
#define SERVO_DRIVER_DEFAULT_FREQ_HZ    50U
#define SERVO_DRIVER_DEFAULT_PERIOD_US  20000U
#define SERVO_DRIVER_DEFAULT_MIN_US     500U
#define SERVO_DRIVER_DEFAULT_CENTER_US  1500U
#define SERVO_DRIVER_DEFAULT_MAX_US     2500U

typedef struct {
    const char *name;
    gpio_num_t gpio;
    ledc_channel_t ledc_channel;
} servo_driver_channel_config_t;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer;
    ledc_timer_bit_t duty_resolution;
    uint32_t frequency_hz;
    uint32_t min_pulse_us;
    uint32_t center_pulse_us;
    uint32_t max_pulse_us;
    const servo_driver_channel_config_t *channels;
    uint8_t channel_count;
} servo_driver_config_t;

void servo_driver_default_config(servo_driver_config_t *config);
esp_err_t servo_driver_init(const servo_driver_config_t *config);
esp_err_t servo_driver_set_angle(uint8_t channel, uint16_t angle_deg);
esp_err_t servo_driver_set_all_angle(uint16_t angle_deg);
uint32_t servo_driver_angle_to_pulse_us(uint16_t angle_deg);
uint32_t servo_driver_pulse_us_to_duty(uint32_t pulse_us);

#ifdef __cplusplus
}
#endif
