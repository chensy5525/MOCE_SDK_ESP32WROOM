/**
 * @file    ssd1315_bridge.c
 * @brief   SSD1315 OLED CH32-CAN-I2C 桥接驱动
 * @note    稳定节点对象由设备发现层传入；本驱动不执行 F0/F1/F2。
 */

#include "ch32_ssd1315_gateway.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSD1315_TAG                     "SSD1315"
#define SSD1315_CONTROL_COMMAND         0x00U
#define SSD1315_CONTROL_DATA            0x40U
#define SSD1315_FONT_GLYPH_COUNT        36U
#define SSD1315_FONT_LETTER_OFFSET      10U
#define SSD1315_ALL_PAGES_MASK          0xFFU
#define SSD1315_COLUMN_START            0U
#define SSD1315_MULTI_TRANSFER_SIZE     (CH32_SSD1315_PAGE_SIZE + 1U)

#define SSD1315_CMD_DISPLAY_OFF         0xAEU
#define SSD1315_CMD_DISPLAY_ON          0xAFU
#define SSD1315_CMD_MEMORY_MODE         0x20U
#define SSD1315_VAL_PAGE_MODE           0x02U
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

static uint32_t ssd1315_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static uint32_t ssd1315_remaining_ms(uint32_t deadline)
{
    uint32_t now = ssd1315_now_ms();
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0U;
}

typedef struct {
    uint8_t data[2];
    uint8_t len;
} ssd1315_command_t;

typedef struct ch32_ssd1315_ctx {
    ch32_i2c_multi_node_t *node;
    uint8_t i2c_addr;
    bool in_use;
    bool initialized;
    uint8_t dirty_pages;
    uint8_t framebuffer[CH32_SSD1315_FRAMEBUFFER_SIZE];
} ch32_ssd1315_ctx_t;

static ch32_ssd1315_ctx_t s_contexts[CH32_SSD1315_MAX_INSTANCES];

static const ssd1315_command_t s_init_commands[] = {
    {{SSD1315_CMD_DISPLAY_OFF, 0U}, 1U},
    {{SSD1315_CMD_MEMORY_MODE, SSD1315_VAL_PAGE_MODE}, 2U},
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
static const uint8_t
    s_font_5x7[SSD1315_FONT_GLYPH_COUNT][CH32_SSD1315_FONT_WIDTH] = {
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

static const uint8_t s_blank_glyph[CH32_SSD1315_FONT_WIDTH] = {
    0U, 0U, 0U, 0U, 0U,
};

static int ssd1315_bridge_write_reg_timeout(ch32_ssd1315_handle_t handle,
                                            const uint8_t *data, size_t len,
                                            const char *operation,
                                            uint32_t timeout_ms)
{
    ch32_i2c_multi_result_t gateway_result;

    if (handle == NULL || data == NULL || len == 0U || len > 4U) {
        return ERR_INVALID_PARAM;
    }

    gateway_result = ch32_i2c_multi_write_reg_to_timeout(
        handle->node, handle->i2c_addr, SSD1315_CONTROL_COMMAND,
        data, (uint8_t)len, timeout_ms);
    if (gateway_result == CH32_I2C_MULTI_RESULT_OK) return 0;
    if (gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT ||
        gateway_result == CH32_I2C_MULTI_RESULT_NO_DATA) {
        SSD1315_LOG_WRN("comm timeout, op=%s node=%u",
                        operation, handle->node->node_id);
        return ERR_TIMEOUT;
    }
    if (gateway_result == CH32_I2C_MULTI_RESULT_INVALID_ARG) {
        return ERR_INVALID_PARAM;
    }
    if (gateway_result == CH32_I2C_MULTI_RESULT_BUSY) return ERR_BUSY;
    return ERR_NO_DEVICE;
}

static int ssd1315_bridge_write_reg(ch32_ssd1315_handle_t handle,
                                    const uint8_t *data, size_t len,
                                    const char *operation)
{
    return ssd1315_bridge_write_reg_timeout(
        handle, data, len, operation, CH32_SSD1315_GATEWAY_TIMEOUT_MS);
}

static int ssd1315_bridge_write_multi(ch32_ssd1315_handle_t handle,
                                      uint8_t page,
                                      const uint8_t *data, size_t len,
                                      uint32_t deadline)
{
    uint8_t page_commands[3];
    uint8_t transfer[SSD1315_MULTI_TRANSFER_SIZE];
    int result;
    ch32_i2c_multi_result_t gateway_result;

    if (handle == NULL || data == NULL || len != CH32_SSD1315_PAGE_SIZE) {
        return ERR_INVALID_PARAM;
    }

    transfer[0] = SSD1315_CONTROL_DATA;
    memcpy(&transfer[1], data, len);
    page_commands[0] = (uint8_t)(SSD1315_CMD_PAGE_BASE | page);
    page_commands[1] = SSD1315_CMD_COLUMN_LOW;
    page_commands[2] = SSD1315_CMD_COLUMN_HIGH;

    uint32_t remaining = ssd1315_remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    result = ssd1315_bridge_write_reg_timeout(
        handle, page_commands, sizeof(page_commands),
        "CMD_WRITE_REG:page_address", remaining);
    if (result != 0) return result;

    remaining = ssd1315_remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    gateway_result = ch32_i2c_multi_write_multi_to_timeout(
        handle->node, handle->i2c_addr, transfer,
        (uint8_t)sizeof(transfer), remaining);
    if (gateway_result == CH32_I2C_MULTI_RESULT_OK) return 0;
    if (gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT ||
        gateway_result == CH32_I2C_MULTI_RESULT_NO_DATA) {
        SSD1315_LOG_WRN("comm timeout, op=CMD_WRITE_MULTI node=%u page=%u",
                        handle->node->node_id, page);
        return ERR_TIMEOUT;
    }
    if (gateway_result == CH32_I2C_MULTI_RESULT_BUSY) return ERR_BUSY;
    return ERR_NO_DEVICE;
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

static int ssd1315_refresh_pages(ch32_ssd1315_handle_t handle,
                                 bool report_display_anomaly)
{
    uint8_t page;
    int result;
    uint32_t deadline = ssd1315_now_ms() +
                        CH32_SSD1315_REFRESH_TIMEOUT_MS;

    if (handle->dirty_pages == 0U) {
        return 0;
    }

    for (page = 0U; page < CH32_SSD1315_PAGE_COUNT; ++page) {
        if ((handle->dirty_pages & (uint8_t)(1U << page)) == 0U) {
            continue;
        }
        result = ssd1315_bridge_write_multi(
            handle, page,
            &handle->framebuffer[(uint16_t)page * CH32_SSD1315_PAGE_SIZE],
            CH32_SSD1315_PAGE_SIZE, deadline);
        if (result != 0) {
            if (report_display_anomaly && result != ERR_TIMEOUT) {
                SSD1315_LOG_ERR("display abnormal, node=%u page=%u err=%d",
                                handle->node->node_id, page, result);
            }
            return result;
        }
        handle->dirty_pages &= (uint8_t)~(1U << page);
    }

    return 0;
}

static ch32_ssd1315_handle_t ssd1315_allocate_context(void)
{
    uint8_t index;

    for (index = 0U; index < CH32_SSD1315_MAX_INSTANCES; ++index) {
        if (!s_contexts[index].in_use) {
            memset(&s_contexts[index], 0, sizeof(s_contexts[index]));
            s_contexts[index].in_use = true;
            return &s_contexts[index];
        }
    }
    return NULL;
}

static void ssd1315_release_instance(ch32_ssd1315_handle_t handle)
{
    if (handle != NULL) {
        memset(handle, 0, sizeof(*handle));
    }
}

int ch32_ssd1315_init(ch32_ssd1315_handle_t *handle,
                      const ch32_ssd1315_cfg_t *cfg)
{
    static const uint8_t s_display_on[] = {SSD1315_CMD_DISPLAY_ON};
    ch32_ssd1315_handle_t instance;
    ch32_i2c_multi_result_t gateway_result;
    bool found = false;
    size_t index;
    int result;

    if (handle == NULL || cfg == NULL) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=invalid_param");
        return ERR_INVALID_PARAM;
    }
    *handle = NULL;
    if (cfg->ch32_node == NULL || !cfg->ch32_node->ready ||
        cfg->ch32_node->token == 0U ||
        cfg->ch32_node->node_id < CH32_SSD1315_I2C_NODE_MIN ||
        cfg->ch32_node->node_id > CH32_SSD1315_I2C_NODE_MAX) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=node_not_assigned node=%u",
                        cfg->ch32_node != NULL ? cfg->ch32_node->node_id : 0U);
        return ERR_INVALID_PARAM;
    }
    if (cfg->i2c_addr != CH32_SSD1315_I2C_ADDR ||
        cfg->clk_speed_hz != CH32_SSD1315_I2C_FREQ_HZ) {
        SSD1315_LOG_ERR("init FAIL, stage=validate reason=invalid_bridge_config");
        return ERR_INVALID_PARAM;
    }
    instance = ssd1315_allocate_context();
    if (instance == NULL) {
        SSD1315_LOG_ERR("init FAIL, stage=allocate reason=busy");
        return ERR_BUSY;
    }

    instance->node = cfg->ch32_node;
    instance->i2c_addr = cfg->i2c_addr;

    vTaskDelay(pdMS_TO_TICKS(CH32_SSD1315_STARTUP_MS));
    gateway_result = ch32_i2c_multi_set_speed_400k(instance->node);
    if (gateway_result != CH32_I2C_MULTI_RESULT_OK) {
        result = gateway_result == CH32_I2C_MULTI_RESULT_TIMEOUT
                     ? ERR_TIMEOUT : ERR_NO_DEVICE;
        SSD1315_LOG_ERR("init FAIL, stage=bus_speed node=%u err=%d",
                        instance->node->node_id, result);
        ssd1315_release_instance(instance);
        return result;
    }

    /* SSD1315 has no readable identity register.  Some controller/bridge
     * combinations do not reliably support an address-only probe even though
     * normal command writes are acknowledged.  Keep the probe diagnostic, but
     * let the first real initialization write be the authoritative presence
     * and transport check. */
    gateway_result = ch32_i2c_multi_probe(instance->node,
                                          instance->i2c_addr, &found);
    if (gateway_result != CH32_I2C_MULTI_RESULT_OK || !found) {
        if (gateway_result == CH32_I2C_MULTI_RESULT_INVALID_ARG) {
            ssd1315_release_instance(instance);
            return ERR_INVALID_PARAM;
        }
        SSD1315_LOG_WRN("probe not confirmed; verify with init write node=%u addr=0x%02X present=%u result=%s",
                        instance->node->node_id, instance->i2c_addr,
                        found ? 1U : 0U,
                        ch32_i2c_multi_result_text(gateway_result));
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
    result = ch32_ssd1315_clear(instance);
    if (result == 0) {
        result = ssd1315_refresh_pages(instance, false);
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

    vTaskDelay(pdMS_TO_TICKS(CH32_SSD1315_STARTUP_MS));
    *handle = instance;
    SSD1315_LOG_INF("init OK, addr=0x%02X, token=0x%04X node=%u",
                    instance->i2c_addr, instance->node->token,
                    instance->node->node_id);
    return 0;
}

int ch32_ssd1315_deinit(ch32_ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->in_use || !handle->initialized) {
        return ERR_NOT_INIT;
    }

    SSD1315_LOG_INF("deinit OK, token=0x%04X node=%u",
                    handle->node->token, handle->node->node_id);
    ssd1315_release_instance(handle);
    return 0;
}

int ch32_ssd1315_clear(ch32_ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }

    memset(handle->framebuffer, 0, sizeof(handle->framebuffer));
    handle->dirty_pages = SSD1315_ALL_PAGES_MASK;
    return 0;
}

int ch32_ssd1315_draw_text(ch32_ssd1315_handle_t handle, uint8_t x,
                           uint8_t page, const char *text)
{
    const uint8_t *glyph;
    uint8_t column;
    uint16_t offset;
    bool changed = false;

    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    if (text == NULL || x >= CH32_SSD1315_WIDTH ||
        page >= CH32_SSD1315_PAGE_COUNT) {
        return ERR_INVALID_PARAM;
    }

    while (*text != '\0') {
        if ((uint16_t)x + CH32_SSD1315_FONT_ADVANCE >
            CH32_SSD1315_WIDTH) {
            return ERR_OVERFLOW;
        }
        glyph = ssd1315_get_glyph(*text);
        for (column = 0U; column < CH32_SSD1315_FONT_WIDTH; ++column) {
            offset = (uint16_t)page * CH32_SSD1315_PAGE_SIZE + x;
            if (handle->framebuffer[offset] != glyph[column]) {
                handle->framebuffer[offset] = glyph[column];
                changed = true;
            }
            ++x;
        }
        offset = (uint16_t)page * CH32_SSD1315_PAGE_SIZE + x;
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

int ch32_ssd1315_refresh(ch32_ssd1315_handle_t handle)
{
    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    return ssd1315_refresh_pages(handle, true);
}
