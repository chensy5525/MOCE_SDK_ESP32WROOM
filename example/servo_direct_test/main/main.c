#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_DIRECT_TEST_COUNT             4U
#define SERVO_DIRECT_TEST_PWM_FREQ_HZ       50U
#define SERVO_DIRECT_TEST_PERIOD_US         20000U
#define SERVO_DIRECT_TEST_TIMER             LEDC_TIMER_1
#define SERVO_DIRECT_TEST_MODE              LEDC_HIGH_SPEED_MODE
#define SERVO_DIRECT_TEST_DUTY_RES          LEDC_TIMER_14_BIT
#define SERVO_DIRECT_TEST_DUTY_MAX          ((1U << 14U) - 1U)
#define SERVO_DIRECT_TEST_MIN_PULSE_US      500U
#define SERVO_DIRECT_TEST_CENTER_PULSE_US   1500U
#define SERVO_DIRECT_TEST_MAX_PULSE_US      2500U
#define SERVO_DIRECT_TEST_STEP_DELAY_MS     1000U

static const char *TAG = "servo_direct_test";

typedef struct {
    const char *name;
    gpio_num_t gpio;
    ledc_channel_t channel;
} servo_direct_test_channel_t;

static const servo_direct_test_channel_t s_servos[SERVO_DIRECT_TEST_COUNT] = {
    {.name = "PWMB1", .gpio = BOARD_PWM_B1_GPIO, .channel = LEDC_CHANNEL_1},
    {.name = "PWMB2", .gpio = BOARD_PWM_B2_GPIO, .channel = LEDC_CHANNEL_2},
    {.name = "PWMB3", .gpio = BOARD_PWM_B3_GPIO, .channel = LEDC_CHANNEL_3},
    {.name = "PWMB4", .gpio = BOARD_PWM_B4_GPIO, .channel = LEDC_CHANNEL_4},
};

static uint32_t angle_to_pulse_us(uint16_t angle_deg)
{
    if (angle_deg > 180U) {
        angle_deg = 180U;
    }

    uint32_t span = SERVO_DIRECT_TEST_MAX_PULSE_US - SERVO_DIRECT_TEST_MIN_PULSE_US;
    return SERVO_DIRECT_TEST_MIN_PULSE_US + ((span * angle_deg) / 180U);
}

static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    return (pulse_us * SERVO_DIRECT_TEST_DUTY_MAX) / SERVO_DIRECT_TEST_PERIOD_US;
}

static esp_err_t servo_direct_test_set_angle(uint8_t index, uint16_t angle_deg)
{
    if (index >= SERVO_DIRECT_TEST_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t pulse_us = angle_to_pulse_us(angle_deg);
    uint32_t duty = pulse_us_to_duty(pulse_us);

    ESP_RETURN_ON_ERROR(
        ledc_set_duty(SERVO_DIRECT_TEST_MODE, s_servos[index].channel, duty),
        TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(
        ledc_update_duty(SERVO_DIRECT_TEST_MODE, s_servos[index].channel),
        TAG, "update duty failed");

    ESP_LOGI(TAG, "ch%u %s GPIO%d angle=%u pulse=%" PRIu32 "us duty=%" PRIu32,
             index, s_servos[index].name, s_servos[index].gpio,
             angle_deg, pulse_us, duty);
    return ESP_OK;
}

static void servo_direct_test_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = SERVO_DIRECT_TEST_MODE,
        .duty_resolution = SERVO_DIRECT_TEST_DUTY_RES,
        .timer_num = SERVO_DIRECT_TEST_TIMER,
        .freq_hz = SERVO_DIRECT_TEST_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    for (uint8_t i = 0U; i < SERVO_DIRECT_TEST_COUNT; ++i) {
        ledc_channel_config_t channel = {
            .gpio_num = s_servos[i].gpio,
            .speed_mode = SERVO_DIRECT_TEST_MODE,
            .channel = s_servos[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = SERVO_DIRECT_TEST_TIMER,
            .duty = pulse_us_to_duty(SERVO_DIRECT_TEST_CENTER_PULSE_US),
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
    }
}

void app_main(void)
{
    static const uint16_t sweep_angles[] = {0U, 90U, 180U, 90U};
    size_t sweep_index = 0U;

    ESP_LOGI(TAG, "ESP32-WROOM servo_direct_test");
    printf("\r\n==== ESP32-WROOM servo_direct_test ====\r\n");
    printf("direct PWM, no CAN, no CH32\r\n");
    printf("ch0=PWMB1/GPIO%d ch1=PWMB2/GPIO%d ch2=PWMB3/GPIO%d ch3=PWMB4/GPIO%d\r\n",
           BOARD_PWM_B1_GPIO, BOARD_PWM_B2_GPIO, BOARD_PWM_B3_GPIO, BOARD_PWM_B4_GPIO);
    printf("servo PWM: %uHz, %uus..%uus..%uus\r\n\r\n",
           SERVO_DIRECT_TEST_PWM_FREQ_HZ,
           SERVO_DIRECT_TEST_MIN_PULSE_US,
           SERVO_DIRECT_TEST_CENTER_PULSE_US,
           SERVO_DIRECT_TEST_MAX_PULSE_US);

    servo_direct_test_init();

    while (true) {
        uint16_t angle = sweep_angles[sweep_index];
        sweep_index = (sweep_index + 1U) % (sizeof(sweep_angles) / sizeof(sweep_angles[0]));

        for (uint8_t i = 0U; i < SERVO_DIRECT_TEST_COUNT; ++i) {
            ESP_ERROR_CHECK(servo_direct_test_set_angle(i, angle));
        }

        printf("servo_direct_test angle=%u ch0=PWMB1 ch1=PWMB2 ch2=PWMB3 ch3=PWMB4\r\n",
               angle);
        vTaskDelay(pdMS_TO_TICKS(SERVO_DIRECT_TEST_STEP_DELAY_MS));
    }
}
