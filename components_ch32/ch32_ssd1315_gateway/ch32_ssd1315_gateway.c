/**
 * @file    ssd1315_bridge.c
 * @brief   SSD1315 OLED CH32-CAN-I2C 桥接驱动
 * @note    稳定节点对象由设备发现层传入；本驱动不执行 F0/F1/F2。
 */

#include "ssd1315.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1315_TAG                     "SSD1315"
#define SSD1315_CONTROL_COMMAND         0x00U
#define SSD1315_CONTROL_DATA            0x40U
#define SSD1315_FONT_GLYPH_COUNT        36U
#define SSD1315_FONT_LETTER_OFFSET      10U
#define SSD1315_ALL_PAGES_MASK          0xFFU
#define SSD1315_COLUMN_START            0U
#define SSD1315_RETRY_DELAY_MS          2U
#define SSD1315_MULTI_TRANSFER_SIZE     (SSD1315_PAGE_SIZE + 1U)

#define SSD1315_CMD_DISPLAY_OFF         0xAEU
#define SSD1315_CMD_DISPLAY_ON          0xAFU
#define SSD1315_CMD_CLOCK_DIV           0xD5U
#define SSD1315_VAL_CLOCK_DIV           0x90U
#define SSD1315_CMD_MULTIPLEX           0xA8U
#define SSD1315_VAL_MULTIPLEX           0x3FU
#define SSD1315_CMD_DISPLAY_OFFSET      0xD3U
#define SSD1315_VAL_DISPLAY_OFFSET      0x00U
#define SSD1315_CMD_START_LINE          0x40U
#define SSD1315_CMD_SEGMENT_REMAP       0xA1U
#define SSD1315_CMD_COM_SCAN_DEC        0xC8U
#define SSD1315_CMD_COM_PINS            0xDAU
#define SSD1315_VAL_COM_PINS            0x12U
#define SSD1315_CMD_CONTRAST            0x81U
#define SSD1315_VAL_CONTRAST            0xB0U
#define SSD1315_CMD_PRECHARGE           0xD9U
#define SSD1315_VAL_PRECHARGE           0x22U
#define SSD1315_CMD_VCOMH               0xDBU
#define SSD1315_VAL_VCOMH               0x30U
#define SSD1315_CMD_RAM_DISPLAY         0xA4U
#define SSD1315_CMD_NORMAL_DISPLAY      0xA6U
#define SSD1315_CMD_CHARGE_PUMP         0x8DU
#define SSD1315_VAL_CHARGE_PUMP_ON      0x14U
#define SSD1315_CMD_PAGE_BASE           0xB0U
#define SSD1315_CMD_COLUMN_LOW          0x00U
#define SSD1315_CMD_COLUMN_HIGH         0x10U

#define SSD1315_LOG_INF(format, ...) \
    printf("[INF][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)
#define SSD1315_LOG_WRN(format, ...) \
    printf("[WRN][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)
#define SSD1315_LOG_ERR(format, ...) \
    printf("[ERR][" SSD1315_TAG "] " format "\n", ##__VA_ARGS__)

typedef struct {
    uint8_t data[2];
    uint8_t len;
} ssd1315_command_t;

typedef struct ssd1315_ctx {
    ch32_i2c_multi_node_t *node;
    uint8_t i2c_addr;
    uint32_t bridge_timeout_ms;
    bool in_use;
    bool initialized;
    bool has_refreshed;
    uint8_t dirty_pages;
    uint8_t framebuffer[SSD1315_FRAMEBUFFER_SIZE];
    TickType_t last_refresh_tick;
    uint32_t ok_count;
    uint32_t error_count;
} ssd1315_ctx_t;

static ssd1315_ctx_t s_contexts[SSD1315_MAX_BRIDGE_INSTANCES];

static const ssd1315_command_t s_init_commands[] = {
    {{SSD1315_CMD_DISPLAY_OFF, 0U}, 1U},
    {{SSD1315_CMD_CLOCK_DIV, SSD1315_VAL_CLOCK_DIV}, 2U},
    {{SSD1315_CMD_MULTIPLEX, SSD1315_VAL_MULTIPLEX}, 2U},
    {{SSD1315_CMD_DISPLAY_OFFSET, SSD1315_VAL_DISPLAY_OFFSET}, 2U},
    {{SSD1315_CMD_START_LINE, 0U}, 1U},
    {{SSD1315_CMD_SEGMENT_REMAP, 0U}, 1U},
    {{SSD1315_CMD_COM_SCAN_DEC, 0U}, 1U},
    {{SSD1315_CMD_COM_PINS, SSD1315_VAL_COM_PINS}, 2U},
    {{SSD1315_CMD_CONTRAST, SSD1315_VAL_CONTRAST}, 2U},
    {{SSD1315_CMD_PRECHARGE, SSD1315_VAL_PRECHARGE}, 2U},
    {{SSD1315_CMD_VCOMH, SSD1315_VAL_VCOMH}, 2U},
    {{SSD1315_CMD_RAM_DISPLAY, 0U}, 1U},
    {{SSD1315_CMD_NORMAL_DISPLAY, 0U}, 1U},
    {{SSD1315_CMD_CHARGE_PUMP, SSD1315_VAL_CHARGE_PUMP_ON}, 2U},
};

/* 5x7 字体：前 10 项为数字，后 26 项为大写字母。 */
static const uint8_t s_font_5x7[SSD1315_FONT_GLYPH_COUNT][SSD1315_FONT_WIDTH] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t s_blank_glyph[SSD1315_FONT_WIDTH] = {0U, 0U, 0U, 0U, 0U};

static int ssd1315_bridge_write_reg(ssd1315_handle_t handle,
                                    const uint8_t *data, size_t len,
                                    const char *operation)
{
    uint32_t attempt;
    int result = ERR_NO_DEVICE;
    ch32_i2c_multi_result_t gateway_result;

    if (handle == NULL || data == NULL || len == 0U || len > 4U) {
        return ERR_INVALID_PARAM;
    }

    for (attempt = 0U; attempt < SSD1315_MAX_RETRIES; ++attempt) {
        gateway_result = ch32_i2c_multi_write_reg_to(
            handle->node, handle->i2c_addr, SSD1315_CONTROL_COMMAND,
            data, (uint8_t)len);
        if (gateway_result == CH32_I2C_MULTI_RESULT_OK) {
            result = 0;
        } else if (gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT ||
                   gateway_result == CH32_I2C_MULTI_RESULT_NO_DATA) {
            result = ERR_TIMEOUT;
        } else if (gateway_result == CH32_I2C_MULTI_RESULT_INVALID_ARG) {
            result = ERR_INVALID_PARAM;
        } else if (gateway_result == CH32_I2C_MULTI_RESULT_BUSY) {
            result = ERR_BUSY;
        } else {
            result = ERR_NO_DEVICE;
        }
        if (result == 0) {
            handle->ok_count++;
            return 0;
        }
        handle->error_count++;
        if (result == ERR_TIMEOUT) {
            SSD1315_LOG_WRN("comm timeout, op=%s node=%u attempt=%lu",
                            operation, handle->node->node_id,
                            (unsigned long)(attempt + 1U));
        }
        if (attempt + 1U < SSD1315_MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(SSD1315_RETRY_DELAY_MS));
        }
    }
    return result;
}

static int ssd1315_bridge_write_multi(ssd1315_handle_t handle, uint8_t page,
                                      const uint8_t *data, size_t len)
{
    uint8_t page_commands[3];
    uint8_t transfer[SSD1315_MULTI_TRANSFER_SIZE];
    uint32_t attempt;
    int result = ERR_NO_DEVICE;
    ch32_i2c_multi_result_t gateway_result;

    if (handle == NULL || data == NULL || len != SSD1315_PAGE_SIZE) {
        return ERR_INVALID_PARAM;
    }

    transfer[0] = SSD1315_CONTROL_DATA;
    memcpy(&transfer[1], data, len);
    page_commands[0] = (uint8_t)(SSD1315_CMD_PAGE_BASE | page);
    page_commands[1] = SSD1315_CMD_COLUMN_LOW;
    page_commands[2] = SSD1315_CMD_COLUMN_HIGH;

    for (attempt = 0U; attempt < SSD1315_MAX_RETRIES; ++attempt) {
        /* 每次重试都重新设置页和列，保证 STOP 后仍可幂等恢复。 */
        result = ssd1315_bridge_write_reg(handle, page_commands,
                                          sizeof(page_commands),
                                          "CMD_WRITE_REG:page_address");
        if (result != 0) {
            handle->error_count++;
            continue;
        }
        gateway_result = ch32_i2c_multi_write_multi_to(
            handle->node, handle->i2c_addr, transfer,
            (uint8_t)sizeof(transfer));
        if (gateway_result == CH32_I2C_MULTI_RESULT_OK) {
            result = 0;
        } else if (gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT ||
                   gateway_result == CH32_I2C_MULTI_RESULT_NO_DATA) {
            result = ERR_TIMEOUT;
        } else if (gateway_result == CH32_I2C_MULTI_RESULT_BUSY) {
            result = ERR_BUSY;
        } else {
            result = ERR_NO_DEVICE;
        }
        if (result == 0) {
            handle->ok_count++;
            return 0;
        }
        handle->error_count++;
        if (result == ERR_TIMEOUT) {
            SSD1315_LOG_WRN("comm timeout, op=CMD_WRITE_MULTI node=%u page=%u attempt=%lu",
                            handle->node->node_id, page,
                            (unsigned long)(attempt + 1U));
        }
        if (attempt + 1U < SSD1315_MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(SSD1315_RETRY_DELAY_MS));
        }
    }
    return result;
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

static int ssd1315_refresh_pages(ssd1315_handle_t handle, bool force,
                                 bool report_display_anomaly)
{
    TickType_t now;
    TickType_t minimum_ticks;
    uint8_t page;
    int result;

    if (handle->dirty_pages == 0U) {
        return 0;
    }

    now = xTaskGetTickCount();
    minimum_ticks = pdMS_TO_TICKS(SSD1315_BRIDGE_REFRESH_INTERVAL_MS);
    if (!force && handle->has_refreshed &&
        (now - handle->last_refresh_tick) < minimum_ticks) {
        return ERR_BUSY;
    }

    for (page = 0U; page < SSD1315_PAGE_COUNT; ++page) {
        if ((handle->dirty_pages & (uint8_t)(1U << page)) == 0U) {
            continue;
        }
        result = ssd1315_bridge_write_multi(
            handle, page,
            &handle->framebuffer[(uint16_t)page * SSD1315_PAGE_SIZE],
            SSD1315_PAGE_SIZE);
        if (result != 0) {
            if (report_display_anomaly && result != ERR_TIMEOUT) {
                SSD1315_LOG_ERR("display abnormal, node=%u page=%u err=%d",
                                handle->node->node_id, page, result);
            }
            return result;
        }
        handle->dirty_pages &= (uint8_t)~(1U << page);
    }

    if (!force) {
        handle->last_refresh_tick = now;
        handle->has_refreshed = true;
    }
    return 0;
}

static bool ssd1315_node_has_address(const ch32_i2c_multi_node_t *node,
                                     uint8_t address)
{
    uint8_t index;

    for (index = 0U; node != NULL && index < node->i2c_addr_count; ++index) {
        if (node->i2c_addrs[index] == address) {
            return true;
        }
    }
    return false;
}

static ssd1315_handle_t ssd1315_allocate_context(void)
{
    uint8_t index;

    for (index = 0U; index < SSD1315_MAX_BRIDGE_INSTANCES; ++index) {
        if (!s_contexts[index].in_use) {
            memset(&s_contexts[index], 0, sizeof(s_contexts[index]));
            s_contexts[index].in_use = true;
            return &s_contexts[index];
        }
    }
    return NULL;
}

static void ssd1315_release_instance(ssd1315_handle_t handle)
{
    if (handle != NULL) {
        memset(handle, 0, sizeof(*handle));
    }
}

int ssd1315_init_device(ssd1315_handle_t *handle, const ssd1315_cfg_t *cfg)
{
    static const uint8_t s_display_on[] = {SSD1315_CMD_DISPLAY_ON};
    ssd1315_handle_t instance;
    ch32_i2c_multi_result_t gateway_result;
    size_t index;
    int result;

    if (handle == NULL || cfg == NULL) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=invalid_param");
        return ERR_INVALID_PARAM;
    }
    *handle = NULL;
    if (cfg->ch32_node == NULL || !cfg->ch32_node->ready ||
        cfg->ch32_node->token == 0U ||
        cfg->ch32_node->node_id < SSD1315_CH32_I2C_NODE_MIN ||
        cfg->ch32_node->node_id > SSD1315_CH32_I2C_NODE_MAX) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=node_not_assigned node=%u",
                        cfg->ch32_node != NULL ? cfg->ch32_node->node_id : 0U);
        return ERR_INVALID_PARAM;
    }
    if (cfg->i2c_addr != SSD1315_I2C_ADDR ||
        cfg->clk_speed_hz != SSD1315_I2C_FREQ_HZ ||
        cfg->bridge_timeout_ms != SSD1315_BRIDGE_TIMEOUT_MS) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=invalid_bridge_config");
        return ERR_INVALID_PARAM;
    }
    if (!ssd1315_node_has_address(cfg->ch32_node, cfg->i2c_addr)) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=downstream_addr_missing node=%u addr=0x%02X",
                        cfg->ch32_node->node_id, cfg->i2c_addr);
        return ERR_NO_DEVICE;
    }

    instance = ssd1315_allocate_context();
    if (instance == NULL) {
        SSD1315_LOG_ERR("init FAIL, stage=allocate reason=busy");
        return ERR_BUSY;
    }

    instance->node = cfg->ch32_node;
    instance->i2c_addr = cfg->i2c_addr;
    instance->bridge_timeout_ms = cfg->bridge_timeout_ms;

    vTaskDelay(pdMS_TO_TICKS(SSD1315_STARTUP_MS));
    gateway_result = ch32_i2c_multi_set_speed_400k(instance->node);
    if (gateway_result != CH32_I2C_MULTI_RESULT_OK) {
        result = gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT
                     ? ERR_TIMEOUT : ERR_NO_DEVICE;
        SSD1315_LOG_ERR("init FAIL, stage=bus_speed node=%u err=%d",
                        instance->node->node_id, result);
        ssd1315_release_instance(instance);
        return result;
    }

    for (index = 0U; index < sizeof(s_init_commands) / sizeof(s_init_commands[0]);
         ++index) {
        result = ssd1315_bridge_write_reg(instance, s_init_commands[index].data,
                                          s_init_commands[index].len,
                                          "CMD_WRITE_REG:init");
        if (result != 0) {
            SSD1315_LOG_ERR("init FAIL, stage=register_config node=%u index=%u err=%d",
                            instance->node->node_id, (unsigned int)index, result);
            ssd1315_release_instance(instance);
            return result;
        }
    }

    instance->initialized = true;
    result = ssd1315_clear_display(instance);
    if (result == 0) {
        result = ssd1315_refresh_pages(instance, true, false);
    }
    if (result == 0) {
        result = ssd1315_bridge_write_reg(instance, s_display_on,
                                          sizeof(s_display_on),
                                          "CMD_WRITE_REG:display_on");
    }
    if (result != 0) {
        SSD1315_LOG_ERR("init FAIL, stage=display_setup node=%u err=%d",
                        instance->node->node_id, result);
        ssd1315_release_instance(instance);
        return result;
    }

    *handle = instance;
    SSD1315_LOG_INF("init OK, addr=0x%02X, token=0x%04X node=%u",
                    instance->i2c_addr, instance->node->token,
                    instance->node->node_id);
    return 0;
}

int ssd1315_deinit_device(ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->in_use || !handle->initialized) {
        return ERR_NOT_INIT;
    }

    SSD1315_LOG_INF("deinit OK, token=0x%04X node=%u",
                    handle->node->token, handle->node->node_id);
    ssd1315_release_instance(handle);
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
    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    return ssd1315_refresh_pages(handle, false, true);
}
