#include "oled_ssd1315_final.h"

#include <stdio.h>
#include <string.h>

#include "bsp_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_ssd1315_font.h"

#define OLED_CONTROL_COMMAND  0x00U
#define OLED_CONTROL_DATA     0x40U
#define OLED_PAGE_COUNT       (OLED_SSD1315_HEIGHT / 8U)

static const uint8_t k_init_commands[] = {
    0xAE, 0x20, 0x02, 0xB0, 0xC8, 0x00, 0x10, 0x40,
    0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
    0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
    0x40, 0x8D, 0x14,
};

static oled_ssd1315_config_t s_config;
static oled_ssd1315_status_t s_status;
static i2c_master_dev_handle_t s_device;
static uint8_t s_framebuffer[OLED_SSD1315_WIDTH * OLED_PAGE_COUNT];

static oled_ssd1315_result_t set_result(oled_ssd1315_result_t result)
{
    s_status.last_result = result;
    if (result != OLED_SSD1315_RESULT_OK &&
        result != OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH) {
        s_status.error_count++;
        s_status.state = OLED_SSD1315_STATE_ERROR;
    }
    return result;
}

static bool write_packet(const uint8_t *data, size_t len)
{
    if (s_device == NULL || data == NULL || len == 0U) {
        return false;
    }
    if (bsp_i2c_write(s_device, data, len, (int)s_config.timeout_ms) != ESP_OK) {
        return false;
    }
    s_status.write_count++;
    return true;
}

static bool write_commands(const uint8_t *commands, size_t len)
{
    uint8_t packet[1U + sizeof(k_init_commands)] = {OLED_CONTROL_COMMAND};
    if (commands == NULL || len == 0U || len > sizeof(k_init_commands)) {
        return false;
    }
    memcpy(&packet[1], commands, len);
    return write_packet(packet, len + 1U);
}

static bool set_cursor(uint8_t page, uint8_t column)
{
    uint8_t commands[3] = {
        (uint8_t)(0xB0U | (page & 0x07U)),
        (uint8_t)(column & 0x0FU),
        (uint8_t)(0x10U | (column >> 4U)),
    };
    return write_commands(commands, sizeof(commands));
}

static void draw_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= OLED_SSD1315_WIDTH || y >= OLED_SSD1315_HEIGHT) {
        return;
    }
    size_t index = (size_t)x + ((size_t)y / 8U) * OLED_SSD1315_WIDTH;
    uint8_t mask = (uint8_t)(1U << (y & 0x07U));
    if (on) {
        s_framebuffer[index] |= mask;
    } else {
        s_framebuffer[index] &= (uint8_t)~mask;
    }
}

static oled_ssd1315_result_t draw_ascii_glyph(uint8_t x, uint8_t y, char ch)
{
    uint8_t columns[5] = {0};
    if (x > OLED_SSD1315_WIDTH - 6U || y > OLED_SSD1315_HEIGHT - 8U) {
        return OLED_SSD1315_RESULT_OUT_OF_BOUNDS;
    }
    bool supported = oled_ssd1315_font_ascii(ch, columns);
    for (uint8_t col = 0; col < 5U; ++col) {
        for (uint8_t row = 0; row < 7U; ++row) {
            draw_pixel((uint8_t)(x + col), (uint8_t)(y + row),
                       (columns[col] & (uint8_t)(1U << row)) != 0U);
        }
    }
    for (uint8_t row = 0; row < 8U; ++row) {
        draw_pixel((uint8_t)(x + 5U), (uint8_t)(y + row), false);
    }
    return supported ? OLED_SSD1315_RESULT_OK
                     : OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH;
}

static oled_ssd1315_result_t draw_chinese_glyph(uint8_t x, uint8_t y,
                                                const char *utf8)
{
    uint16_t rows[16] = {0};
    if (x > OLED_SSD1315_WIDTH - 16U || y > OLED_SSD1315_HEIGHT - 16U) {
        return OLED_SSD1315_RESULT_OUT_OF_BOUNDS;
    }
    if (!oled_ssd1315_font_chinese(utf8, 3U, rows)) {
        return OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH;
    }
    for (uint8_t row = 0; row < 16U; ++row) {
        for (uint8_t col = 0; col < 16U; ++col) {
            draw_pixel((uint8_t)(x + col), (uint8_t)(y + row),
                       (rows[row] & (uint16_t)(1U << (15U - col))) != 0U);
        }
    }
    return OLED_SSD1315_RESULT_OK;
}

void oled_ssd1315_default_config(oled_ssd1315_config_t *config)
{
    if (config == NULL) {
        return;
    }
    config->address = OLED_SSD1315_DEFAULT_ADDR7;
    config->i2c_speed_hz = OLED_SSD1315_DEFAULT_SPEED_HZ;
    config->timeout_ms = 300U;
}

oled_ssd1315_result_t oled_ssd1315_init(const oled_ssd1315_config_t *config)
{
    oled_ssd1315_config_t local;
    if (config == NULL) {
        oled_ssd1315_default_config(&local);
        config = &local;
    }
    if (config->address > 0x7FU || config->i2c_speed_hz == 0U) {
        return OLED_SSD1315_RESULT_BAD_ARG;
    }

    if (s_device != NULL) {
        (void)bsp_i2c_remove_device(s_device);
        s_device = NULL;
    }

    s_config = *config;
    memset(&s_status, 0, sizeof(s_status));
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    s_status.address = s_config.address;
    s_status.state = OLED_SSD1315_STATE_RESET;
    s_status.last_result = OLED_SSD1315_RESULT_NOT_READY;

    if (bsp_i2c_init() != ESP_OK) {
        return set_result(OLED_SSD1315_RESULT_COMM_FAIL);
    }
    s_status.i2c_ready = true;
    s_status.state = OLED_SSD1315_STATE_I2C_READY;

    vTaskDelay(pdMS_TO_TICKS(20));
    if (bsp_i2c_probe(s_config.address, (int)s_config.timeout_ms) != ESP_OK) {
        /* Some SSD1315 modules strap SA0 high and answer at 0x3D. */
        if (s_config.address == OLED_SSD1315_DEFAULT_ADDR7 &&
            bsp_i2c_probe(0x3DU, (int)s_config.timeout_ms) == ESP_OK) {
            s_config.address = 0x3DU;
            s_status.address = s_config.address;
        } else {
            return set_result(OLED_SSD1315_RESULT_ADDR_NOT_FOUND);
        }
    }
    s_status.device_found = true;
    s_status.state = OLED_SSD1315_STATE_DEVICE_FOUND;

    if (bsp_i2c_add_device_7bit(s_config.address, s_config.i2c_speed_hz,
                                &s_device) != ESP_OK ||
        !write_commands(k_init_commands, sizeof(k_init_commands))) {
        return set_result(OLED_SSD1315_RESULT_INIT_FAIL);
    }

    s_status.initialized = true;
    s_status.state = OLED_SSD1315_STATE_READY;
    if (oled_ssd1315_refresh() != OLED_SSD1315_RESULT_OK ||
        oled_ssd1315_set_display(true) != OLED_SSD1315_RESULT_OK) {
        s_status.initialized = false;
        return set_result(OLED_SSD1315_RESULT_INIT_FAIL);
    }
    return set_result(OLED_SSD1315_RESULT_OK);
}

oled_ssd1315_result_t oled_ssd1315_clear(void)
{
    if (!s_status.initialized) {
        return set_result(OLED_SSD1315_RESULT_NOT_READY);
    }
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    return set_result(OLED_SSD1315_RESULT_OK);
}

oled_ssd1315_result_t oled_ssd1315_draw_ascii(uint8_t x, uint8_t y,
                                              const char *text)
{
    if (!s_status.initialized) {
        return set_result(OLED_SSD1315_RESULT_NOT_READY);
    }
    if (text == NULL) {
        return set_result(OLED_SSD1315_RESULT_BAD_ARG);
    }

    oled_ssd1315_result_t result = OLED_SSD1315_RESULT_OK;
    while (*text != '\0') {
        oled_ssd1315_result_t glyph = draw_ascii_glyph(x, y, *text++);
        if (glyph == OLED_SSD1315_RESULT_OUT_OF_BOUNDS) {
            return set_result(glyph);
        }
        if (glyph == OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH) {
            result = glyph;
        }
        x = (uint8_t)(x + 6U);
    }
    return set_result(result);
}

oled_ssd1315_result_t oled_ssd1315_draw_utf8(uint8_t x, uint8_t y,
                                             const char *text)
{
    if (!s_status.initialized) {
        return set_result(OLED_SSD1315_RESULT_NOT_READY);
    }
    if (text == NULL) {
        return set_result(OLED_SSD1315_RESULT_BAD_ARG);
    }

    oled_ssd1315_result_t result = OLED_SSD1315_RESULT_OK;
    const uint8_t *cursor = (const uint8_t *)text;
    while (*cursor != 0U) {
        oled_ssd1315_result_t glyph;
        if (*cursor < 0x80U) {
            glyph = draw_ascii_glyph(x, y, (char)*cursor);
            cursor++;
            x = (uint8_t)(x + 6U);
        } else if ((*cursor & 0xF0U) == 0xE0U &&
                   cursor[1] != 0U && cursor[2] != 0U) {
            glyph = draw_chinese_glyph(x, y, (const char *)cursor);
            cursor += 3;
            x = (uint8_t)(x + 16U);
        } else {
            return set_result(OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH);
        }
        if (glyph == OLED_SSD1315_RESULT_OUT_OF_BOUNDS) {
            return set_result(glyph);
        }
        if (glyph == OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH) {
            result = glyph;
        }
    }
    return set_result(result);
}

oled_ssd1315_result_t oled_ssd1315_draw_uint(uint8_t x, uint8_t y,
                                             uint32_t value)
{
    char text[11];
    (void)snprintf(text, sizeof(text), "%lu", (unsigned long)value);
    return oled_ssd1315_draw_ascii(x, y, text);
}

oled_ssd1315_result_t oled_ssd1315_refresh(void)
{
    if (!s_status.initialized) {
        return set_result(OLED_SSD1315_RESULT_NOT_READY);
    }

    uint8_t packet[OLED_SSD1315_WIDTH + 1U];
    packet[0] = OLED_CONTROL_DATA;
    for (uint8_t page = 0; page < OLED_PAGE_COUNT; ++page) {
        if (!set_cursor(page, 0U)) {
            return set_result(OLED_SSD1315_RESULT_COMM_FAIL);
        }
        memcpy(&packet[1], &s_framebuffer[(size_t)page * OLED_SSD1315_WIDTH],
               OLED_SSD1315_WIDTH);
        if (!write_packet(packet, sizeof(packet))) {
            return set_result(OLED_SSD1315_RESULT_COMM_FAIL);
        }
    }
    s_status.refresh_count++;
    s_status.state = OLED_SSD1315_STATE_READY;
    return set_result(OLED_SSD1315_RESULT_OK);
}

oled_ssd1315_result_t oled_ssd1315_set_display(bool on)
{
    if (!s_status.initialized) {
        return set_result(OLED_SSD1315_RESULT_NOT_READY);
    }
    uint8_t command = on ? 0xAFU : 0xAEU;
    if (!write_commands(&command, 1U)) {
        return set_result(OLED_SSD1315_RESULT_COMM_FAIL);
    }
    s_status.display_on = on;
    return set_result(OLED_SSD1315_RESULT_OK);
}

void oled_ssd1315_get_status(oled_ssd1315_status_t *status)
{
    if (status != NULL) {
        *status = s_status;
    }
}

const char *oled_ssd1315_state_text(oled_ssd1315_state_t state)
{
    switch (state) {
    case OLED_SSD1315_STATE_RESET: return "RESET";
    case OLED_SSD1315_STATE_I2C_READY: return "I2C_READY";
    case OLED_SSD1315_STATE_DEVICE_FOUND: return "DEVICE_FOUND";
    case OLED_SSD1315_STATE_READY: return "READY";
    case OLED_SSD1315_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

const char *oled_ssd1315_result_text(oled_ssd1315_result_t result)
{
    switch (result) {
    case OLED_SSD1315_RESULT_OK: return "OK";
    case OLED_SSD1315_RESULT_ADDR_NOT_FOUND: return "ADDR_NOT_FOUND";
    case OLED_SSD1315_RESULT_INIT_FAIL: return "INIT_FAIL";
    case OLED_SSD1315_RESULT_COMM_FAIL: return "COMM_FAIL";
    case OLED_SSD1315_RESULT_NOT_READY: return "NOT_READY";
    case OLED_SSD1315_RESULT_BAD_ARG: return "BAD_ARG";
    case OLED_SSD1315_RESULT_OUT_OF_BOUNDS: return "OUT_OF_BOUNDS";
    case OLED_SSD1315_RESULT_UNSUPPORTED_GLYPH: return "UNSUPPORTED_GLYPH";
    default: return "UNKNOWN";
    }
}
