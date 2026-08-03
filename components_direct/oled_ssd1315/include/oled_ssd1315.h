#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_SSD1315_WIDTH              128U
#define OLED_SSD1315_HEIGHT             64U
#define OLED_SSD1315_DEFAULT_ADDR7       0x3CU
#define OLED_SSD1315_DEFAULT_SPEED_HZ    400000U

typedef enum {
    OLED_SSD1315_STATE_RESET = 0,
    OLED_SSD1315_STATE_I2C_READY,
    OLED_SSD1315_STATE_DEVICE_FOUND,
    OLED_SSD1315_STATE_READY,
    OLED_SSD1315_STATE_ERROR,
} oled_ssd1315_state_t;

typedef enum {
    OLED_SSD1315_RESULT_OK = 0,
    OLED_SSD1315_RESULT_ADDR_NOT_FOUND,
    OLED_SSD1315_RESULT_INIT_FAIL,
    OLED_SSD1315_RESULT_COMM_FAIL,
    OLED_SSD1315_RESULT_NOT_READY,
    OLED_SSD1315_RESULT_BAD_ARG,
    OLED_SSD1315_RESULT_OUT_OF_BOUNDS,
    OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH,
} oled_ssd1315_result_t;

typedef struct {
    uint8_t address;
    uint32_t i2c_speed_hz;
    uint32_t timeout_ms;
} oled_ssd1315_config_t;

typedef struct {
    oled_ssd1315_state_t state;
    oled_ssd1315_result_t last_result;
    uint8_t address;
    bool i2c_ready;
    bool device_found;
    bool initialized;
    bool display_on;
    uint32_t write_count;
    uint32_t refresh_count;
    uint32_t error_count;
} oled_ssd1315_status_t;

void oled_ssd1315_default_config(oled_ssd1315_config_t *config);
oled_ssd1315_result_t oled_ssd1315_init(const oled_ssd1315_config_t *config);
oled_ssd1315_result_t oled_ssd1315_clear(void);
oled_ssd1315_result_t oled_ssd1315_draw_ascii(uint8_t x, uint8_t y,
                                              const char *text);
oled_ssd1315_result_t oled_ssd1315_draw_utf8(uint8_t x, uint8_t y,
                                             const char *text);
oled_ssd1315_result_t oled_ssd1315_draw_uint(uint8_t x, uint8_t y,
                                             uint32_t value);
oled_ssd1315_result_t oled_ssd1315_refresh(void);
oled_ssd1315_result_t oled_ssd1315_set_display(bool on);
void oled_ssd1315_get_status(oled_ssd1315_status_t *status);
const char *oled_ssd1315_state_text(oled_ssd1315_state_t state);
const char *oled_ssd1315_result_text(oled_ssd1315_result_t result);

#ifdef __cplusplus
}
#endif
