/**
 * @file    ssd1315_direct.c
 * @brief   SSD1315 OLED 直连驱动实现
 * @note    芯片型号：SSD1315 / 接口：I2C / 连接方式：ESP32 直连
 */

#include "ssd1315.h"

#include <stdio.h>
#include <string.h>

#include "bsp_i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1315_TAG                 "SSD1315"
#define SSD1315_CONTROL_COMMAND     0x00U
#define SSD1315_CONTROL_DATA        0x40U
#define SSD1315_COMMAND_MAX_SIZE    32U
#define SSD1315_TRANSFER_MAX_SIZE   (SSD1315_PAGE_SIZE + 1U)
#define SSD1315_PAGE_COMMAND_SIZE   3U
#define SSD1315_FONT_GLYPH_COUNT    36U
#define SSD1315_FONT_LETTER_OFFSET  10U
#define SSD1315_RETRY_DELAY_MS      1U
#define SSD1315_CMD_DISPLAY_OFF     0xAEU
#define SSD1315_CMD_DISPLAY_ON      0xAFU
#define SSD1315_CMD_CLOCK_DIV       0xD5U
#define SSD1315_VAL_CLOCK_DIV       0x90U
#define SSD1315_CMD_MULTIPLEX       0xA8U
#define SSD1315_VAL_MULTIPLEX       0x3FU
#define SSD1315_CMD_DISPLAY_OFFSET  0xD3U
#define SSD1315_VAL_DISPLAY_OFFSET  0x00U
#define SSD1315_CMD_START_LINE      0x40U
#define SSD1315_CMD_SEGMENT_REMAP   0xA1U
#define SSD1315_CMD_COM_SCAN_DEC    0xC8U
#define SSD1315_CMD_COM_PINS        0xDAU
#define SSD1315_VAL_COM_PINS        0x12U
#define SSD1315_CMD_CONTRAST        0x81U
#define SSD1315_VAL_CONTRAST        0xB0U
#define SSD1315_CMD_PRECHARGE       0xD9U
#define SSD1315_VAL_PRECHARGE       0x22U
#define SSD1315_CMD_VCOMH           0xDBU
#define SSD1315_VAL_VCOMH           0x30U
#define SSD1315_CMD_RAM_DISPLAY     0xA4U
#define SSD1315_CMD_NORMAL_DISPLAY  0xA6U
#define SSD1315_CMD_CHARGE_PUMP     0x8DU
#define SSD1315_VAL_CHARGE_PUMP_ON  0x14U
#define SSD1315_CMD_PAGE_BASE       0xB0U
#define SSD1315_CMD_COLUMN_LOW      0x00U
#define SSD1315_CMD_COLUMN_HIGH     0x10U
#define SSD1315_ALL_PAGES_MASK      0xFFU

/* 日志统一从调试串口输出固定格式。 */
#define SSD1315_LOG_INF(format, ...) \
    printf("[INF][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)
#define SSD1315_LOG_WRN(format, ...) \
    printf("[WRN][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)
#define SSD1315_LOG_ERR(format, ...) \
    printf("[ERR][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)

/* ================================================================
 * 模块句柄结构体
 * ================================================================ */
typedef struct ssd1315_ctx {
    i2c_master_dev_handle_t i2c_device;
    uint8_t i2c_addr;
    bool initialized;
    uint8_t dirty_pages;
    uint8_t framebuffer[SSD1315_FRAMEBUFFER_SIZE];
    uint32_t ok_count;
    uint32_t error_count;
} ssd1315_ctx_t;

/* 禁止动态内存，因此当前直连驱动提供一个静态实例。 */
static ssd1315_ctx_t s_ctx;
static bool s_ctx_in_use;

/* 5x7 字体：前 10 项为数字，后 26 项为大写字母。 */
static const uint8_t s_font_5x7[SSD1315_FONT_GLYPH_COUNT][SSD1315_FONT_WIDTH] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t s_blank_glyph[SSD1315_FONT_WIDTH] = {0U, 0U, 0U, 0U, 0U};

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

static int ssd1315_map_esp_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return 0;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return ERR_TIMEOUT;
    }
    if (error == ESP_ERR_INVALID_ARG) {
        return ERR_INVALID_PARAM;
    }
    return ERR_NO_DEVICE;
}

static int ssd1315_write_i2c(ssd1315_handle_t handle, uint8_t control,
                             const uint8_t *data, size_t len)
{
    uint8_t transfer[SSD1315_TRANSFER_MAX_SIZE];
    esp_err_t error = ESP_FAIL;
    uint32_t attempt;

    if (handle == NULL || data == NULL || len == 0U) {
        return ERR_INVALID_PARAM;
    }
    if (len > SSD1315_PAGE_SIZE) {
        return ERR_OVERFLOW;
    }

    transfer[0] = control;
    memcpy(&transfer[1], data, len);

    for (attempt = 0U; attempt < SSD1315_MAX_RETRIES; ++attempt) {
        error = bsp_i2c_write(handle->i2c_device, transfer, len + 1U,
                              (int)SSD1315_I2C_TIMEOUT_MS);
        if (error == ESP_OK) {
            handle->ok_count++;
            return 0;
        }
        handle->error_count++;
        if (attempt + 1U < SSD1315_MAX_RETRIES) {
            SSD1315_LOG_WRN("i2c retry=%lu", (unsigned long)(attempt + 1U));
            vTaskDelay(pdMS_TO_TICKS(SSD1315_RETRY_DELAY_MS));
        }
    }

    return ssd1315_map_esp_error(error);
}

static int ssd1315_write_commands(ssd1315_handle_t handle,
                                  const uint8_t *commands, size_t len)
{
    if (len > SSD1315_COMMAND_MAX_SIZE) {
        return ERR_OVERFLOW;
    }
    return ssd1315_write_i2c(handle, SSD1315_CONTROL_COMMAND, commands, len);
}

static int ssd1315_write_data(ssd1315_handle_t handle,
                              const uint8_t *data, size_t len)
{
    return ssd1315_write_i2c(handle, SSD1315_CONTROL_DATA, data, len);
}

static const uint8_t *ssd1315_get_glyph(char character)
{
    uint8_t index;

    if (character >= 'a' && character <= 'z') {
        character = (char)(character - ('a' - 'A'));
    }
    if (character >= '0' && character <= '9') {
        index = (uint8_t)(character - '0');
        return s_font_5x7[index];
    }
    if (character >= 'A' && character <= 'Z') {
        index = (uint8_t)(SSD1315_FONT_LETTER_OFFSET + character - 'A');
        return s_font_5x7[index];
    }
    return s_blank_glyph;
}

static int ssd1315_configure_i2c_device(ssd1315_handle_t handle,
                                        const ssd1315_cfg_t *cfg)
{
    esp_err_t error;

    if (cfg->initialize_i2c || !bsp_i2c_is_initialized()) {
        error = bsp_i2c_init();
        if (error != ESP_OK) {
            return ssd1315_map_esp_error(error);
        }
    }
    error = bsp_i2c_add_device_7bit(cfg->i2c_addr, cfg->clk_speed_hz,
                                    &handle->i2c_device);
    return ssd1315_map_esp_error(error);
}

/* ================================================================
 * API 实现
 * ================================================================ */

int ssd1315_init_device(ssd1315_handle_t *handle, const ssd1315_cfg_t *cfg)
{
    static const uint8_t s_init_commands[] = {
        SSD1315_CMD_DISPLAY_OFF,
        SSD1315_CMD_CLOCK_DIV, SSD1315_VAL_CLOCK_DIV,
        SSD1315_CMD_MULTIPLEX, SSD1315_VAL_MULTIPLEX,
        SSD1315_CMD_DISPLAY_OFFSET, SSD1315_VAL_DISPLAY_OFFSET,
        SSD1315_CMD_START_LINE,
        SSD1315_CMD_SEGMENT_REMAP,
        SSD1315_CMD_COM_SCAN_DEC,
        SSD1315_CMD_COM_PINS, SSD1315_VAL_COM_PINS,
        SSD1315_CMD_CONTRAST, SSD1315_VAL_CONTRAST,
        SSD1315_CMD_PRECHARGE, SSD1315_VAL_PRECHARGE,
        SSD1315_CMD_VCOMH, SSD1315_VAL_VCOMH,
        SSD1315_CMD_RAM_DISPLAY,
        SSD1315_CMD_NORMAL_DISPLAY,
        SSD1315_CMD_CHARGE_PUMP, SSD1315_VAL_CHARGE_PUMP_ON,
    };
    static const uint8_t s_display_on_command = SSD1315_CMD_DISPLAY_ON;
    int result;

    if (handle == NULL || cfg == NULL) {
        SSD1315_LOG_ERR("init FAIL, reason=invalid_param");
        return ERR_INVALID_PARAM;
    }
    *handle = NULL;
    if (cfg->i2c_addr != SSD1315_I2C_ADDR ||
        cfg->clk_speed_hz != SSD1315_I2C_FREQ_HZ) {
        SSD1315_LOG_ERR("init FAIL, reason=invalid_i2c_config");
        return ERR_INVALID_PARAM;
    }
    if (s_ctx_in_use) {
        SSD1315_LOG_ERR("init FAIL, reason=busy");
        return ERR_BUSY;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.i2c_addr = cfg->i2c_addr;
    s_ctx_in_use = true;

    result = ssd1315_configure_i2c_device(&s_ctx, cfg);
    if (result != 0) {
        s_ctx_in_use = false;
        SSD1315_LOG_ERR("init FAIL, reason=i2c_config err=%d", result);
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(SSD1315_STARTUP_MS));

    result = ssd1315_write_commands(&s_ctx, s_init_commands,
                                    sizeof(s_init_commands));
    if (result != 0) {
        (void)bsp_i2c_remove_device(s_ctx.i2c_device);
        memset(&s_ctx, 0, sizeof(s_ctx));
        s_ctx_in_use = false;
        SSD1315_LOG_ERR("init FAIL, reason=no_device err=%d", result);
        return result;
    }

    s_ctx.initialized = true;
    result = ssd1315_clear_display(&s_ctx);
    if (result == 0) {
        result = ssd1315_refresh_display(&s_ctx);
    }
    if (result == 0) {
        result = ssd1315_write_commands(&s_ctx, &s_display_on_command,
                                        sizeof(s_display_on_command));
    }
    if (result != 0) {
        (void)bsp_i2c_remove_device(s_ctx.i2c_device);
        memset(&s_ctx, 0, sizeof(s_ctx));
        s_ctx_in_use = false;
        SSD1315_LOG_ERR("init FAIL, reason=display_setup err=%d", result);
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(SSD1315_STARTUP_MS));
    *handle = &s_ctx;
    SSD1315_LOG_INF("init OK, addr=0x%02X", s_ctx.i2c_addr);
    return 0;
}

int ssd1315_deinit_device(ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->initialized || handle != &s_ctx) {
        return ERR_NOT_INIT;
    }

    handle->initialized = false;
    (void)bsp_i2c_remove_device(handle->i2c_device);
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx_in_use = false;
    SSD1315_LOG_INF("deinit OK");
    return 0;
}

int ssd1315_clear_display(ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }

    memset(handle->framebuffer, 0, sizeof(handle->framebuffer));
    handle->dirty_pages = SSD1315_ALL_PAGES_MASK;
    return 0;
}

int ssd1315_draw_text(ssd1315_handle_t handle, uint8_t x, uint8_t page,
                      const char *text)
{
    const uint8_t *glyph;
    uint8_t column;
    uint16_t offset;
    bool changed = false;

    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    if (text == NULL || x >= SSD1315_WIDTH || page >= SSD1315_PAGE_COUNT) {
        return ERR_INVALID_PARAM;
    }

    while (*text != '\0') {
        if ((uint16_t)x + SSD1315_FONT_ADVANCE > SSD1315_WIDTH) {
            return ERR_OVERFLOW;
        }
        glyph = ssd1315_get_glyph(*text);
        for (column = 0U; column < SSD1315_FONT_WIDTH; ++column) {
            offset = (uint16_t)page * SSD1315_PAGE_SIZE + x;
            if (handle->framebuffer[offset] != glyph[column]) {
                handle->framebuffer[offset] = glyph[column];
                changed = true;
            }
            ++x;
        }
        offset = (uint16_t)page * SSD1315_PAGE_SIZE + x;
        if (handle->framebuffer[offset] != 0U) {
            handle->framebuffer[offset] = 0U;
            changed = true;
        }
        ++x;
        ++text;
    }

    if (changed) {
        handle->dirty_pages |= (uint8_t)(1U << page);
    }

    return 0;
}

int ssd1315_refresh_display(ssd1315_handle_t handle)
{
    uint8_t page;
    uint8_t page_commands[SSD1315_PAGE_COMMAND_SIZE];
    int result;

    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    if (handle->dirty_pages == 0U) {
        return 0;
    }

    for (page = 0U; page < SSD1315_PAGE_COUNT; ++page) {
        if ((handle->dirty_pages & (uint8_t)(1U << page)) == 0U) {
            continue;
        }
        page_commands[0] = (uint8_t)(SSD1315_CMD_PAGE_BASE | page);
        page_commands[1] = SSD1315_CMD_COLUMN_LOW;
        page_commands[2] = SSD1315_CMD_COLUMN_HIGH;

        result = ssd1315_write_commands(handle, page_commands,
                                        sizeof(page_commands));
        if (result != 0) {
            SSD1315_LOG_ERR("refresh FAIL, page=%u err=%d", page, result);
            return result;
        }
        result = ssd1315_write_data(
            handle,
            &handle->framebuffer[(uint16_t)page * SSD1315_PAGE_SIZE],
            SSD1315_PAGE_SIZE);
        if (result != 0) {
            SSD1315_LOG_ERR("refresh FAIL, page=%u err=%d", page, result);
            return result;
        }
        handle->dirty_pages &= (uint8_t)~(1U << page);
    }

    return 0;
}
