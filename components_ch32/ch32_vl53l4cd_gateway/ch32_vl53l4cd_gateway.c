#include "ch32_vl53l4cd_gateway.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L4CD"
#define REG_OSC_FREQUENCY 0x0006U
#define REG_VHV_TIMEOUT 0x0008U
#define REG_GPIO_MUX 0x0030U
#define REG_GPIO_STATUS 0x0031U
#define REG_RANGE_CONFIG_A 0x005EU
#define REG_RANGE_CONFIG_B 0x0061U
#define REG_INTERMEASUREMENT 0x006CU
#define REG_INTERRUPT_CLEAR 0x0086U
#define REG_SYSTEM_START 0x0087U
#define REG_RANGE_STATUS 0x0089U
#define REG_DISTANCE 0x0096U
#define REG_FIRMWARE_STATUS 0x00E5U
#define REG_MODEL_ID 0x010FU
#define POLL_MS 10U
#define LOG_INF(f,...) printf("[INF][" TAG "] " f "\n",##__VA_ARGS__)
#define LOG_ERR(f,...) printf("[ERR][" TAG "] " f "\n",##__VA_ARGS__)
#define TRY(expression)          \
    do {                         \
        int result_ = (expression); \
        if (result_ != 0) {      \
            return result_;      \
        }                        \
    } while (0)

typedef struct ch32_vl53l4cd_ctx {
    bool in_use;
    bool initialized;
    bool ready_level;
    ch32_i2c_multi_node_t *node;
    uint8_t addr;
    uint32_t ready_timeout_ms;
} ch32_vl53l4cd_ctx_t;

static ch32_vl53l4cd_ctx_t s_instances[CH32_VL53L4CD_MAX_INSTANCES];
static const uint8_t s_default_config[] = {
    0x12, 0x00, 0x00, 0x11, 0x02, 0x00, 0x02, 0x08,
    0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x0B, 0x00, 0x00, 0x02, 0x14, 0x21,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xC8,
    0x00, 0x00, 0x38, 0xFF, 0x01, 0x00, 0x08, 0x00,
    0x00, 0x01, 0xCC, 0x07, 0x01, 0xF1, 0x05, 0x00,
    0xA0, 0x00, 0x80, 0x08, 0x38, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x07, 0x05, 0x06, 0x06, 0x00,
    0x00, 0x02, 0xC7, 0xFF, 0x9B, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00,
};

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static uint32_t remaining_ms(uint32_t deadline)
{
    uint32_t now = now_ms();

    return (int32_t)(deadline - now) > 0 ? deadline - now : 0U;
}

static void delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);

    vTaskDelay(ticks != 0U ? ticks : 1U);
}

static int valid_node(const ch32_i2c_multi_node_t *node)
{
    if (node == NULL || !node->ready || node->token == 0U ||
        node->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C ||
        node->node_id < 1U || node->node_id > 0x20U) {
        return ERR_INVALID_PARAM;
    }
    return 0;
}

static int map_gateway(ch32_i2c_multi_result_t result)
{
    switch (result) {
    case CH32_I2C_MULTI_RESULT_OK:
        return 0;
    case CH32_I2C_MULTI_RESULT_TIMEOUT:
        return ERR_TIMEOUT;
    case CH32_I2C_MULTI_RESULT_BUSY:
        return ERR_BUSY;
    case CH32_I2C_MULTI_RESULT_INVALID_ARG:
        return ERR_INVALID_PARAM;
    case CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND:
        return ERR_NO_DEVICE;
    default:
        return ERR_CH32_VL53L4CD_COMM;
    }
}
static int write_data(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg,
                      const uint8_t *data, size_t len)
{
    uint8_t buffer[136];

    if (len + 2U > sizeof(buffer)) return ERR_OVERFLOW;
    if (valid_node(ctx->node) != 0) return ERR_NO_DEVICE;
    buffer[0] = (uint8_t)(reg >> 8U);
    buffer[1] = (uint8_t)reg;
    memcpy(&buffer[2], data, len);
    return map_gateway(ch32_i2c_multi_write_multi_to(
        ctx->node, ctx->addr, buffer, (uint8_t)(len + 2U)));
}

static int write_data_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg,
                              const uint8_t *data, size_t len,
                              uint32_t timeout_ms)
{
    uint8_t buffer[136];

    if (len + 2U > sizeof(buffer) || timeout_ms == 0U) return ERR_OVERFLOW;
    if (valid_node(ctx->node) != 0) return ERR_NO_DEVICE;
    buffer[0] = (uint8_t)(reg >> 8U);
    buffer[1] = (uint8_t)reg;
    memcpy(&buffer[2], data, len);
    return map_gateway(ch32_i2c_multi_write_multi_to_timeout(
        ctx->node, ctx->addr, buffer, (uint8_t)(len + 2U), timeout_ms));
}

static int read_data_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg,
                             uint8_t *data, uint8_t len,
                             uint32_t timeout_ms)
{
    uint8_t index[2] = {(uint8_t)(reg >> 8U), (uint8_t)reg};

    if (valid_node(ctx->node) != 0) {
        return ERR_NO_DEVICE;
    }
    return map_gateway(ch32_i2c_multi_write_read_to_timeout(
        ctx->node, ctx->addr, index, sizeof(index), data, len, timeout_ms));
}

static int w8(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t value)
{
    return write_data(ctx, reg, &value, 1U);
}

static int w16_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint16_t value,
                       uint32_t timeout_ms)
{
    uint8_t data[2] = {(uint8_t)(value >> 8U), (uint8_t)value};
    return write_data_timeout(ctx, reg, data, sizeof(data), timeout_ms);
}

static int w32_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint32_t value,
                       uint32_t timeout_ms)
{
    uint8_t data[4] = {
        (uint8_t)(value >> 24U), (uint8_t)(value >> 16U),
        (uint8_t)(value >> 8U), (uint8_t)value,
    };
    return write_data_timeout(ctx, reg, data, sizeof(data), timeout_ms);
}

static int r8_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t *value,
                      uint32_t timeout_ms)
{
    return read_data_timeout(ctx, reg, value, 1U, timeout_ms);
}

static int r16_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint16_t *value,
                       uint32_t timeout_ms)
{
    uint8_t data[2];

    TRY(read_data_timeout(ctx, reg, data, sizeof(data), timeout_ms));
    *value = ((uint16_t)data[0] << 8U) | data[1];
    return 0;
}

static int w8_timeout(ch32_vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t value,
                      uint32_t timeout_ms)
{
    return write_data_timeout(ctx, reg, &value, 1U, timeout_ms);
}

static int wait_ready_until(ch32_vl53l4cd_ctx_t *ctx, uint32_t deadline)
{
    while (true) {
        uint32_t remaining = remaining_ms(deadline);
        uint8_t status;

        if (remaining == 0U) {
            return ERR_CH32_VL53L4CD_DATA_NOT_READY;
        }
        TRY(r8_timeout(ctx, REG_GPIO_STATUS, &status, remaining));
        if (((status & 1U) != 0U) == ctx->ready_level) {
            return 0;
        }
        remaining = remaining_ms(deadline);
        if (remaining == 0U) {
            return ERR_CH32_VL53L4CD_DATA_NOT_READY;
        }
        delay_ms(remaining < POLL_MS ? remaining : POLL_MS);
    }
}

static int wait_ready(ch32_vl53l4cd_ctx_t *ctx, uint32_t timeout_ms)
{
    return wait_ready_until(ctx, now_ms() + timeout_ms);
}

static int wait_firmware_ready(ch32_vl53l4cd_ctx_t *ctx, uint32_t timeout_ms)
{
    uint32_t deadline = now_ms() + timeout_ms;

    while (true) {
        uint32_t remaining = remaining_ms(deadline);
        uint8_t firmware_status;

        if (remaining == 0U) {
            return ERR_TIMEOUT;
        }
        TRY(r8_timeout(ctx, REG_FIRMWARE_STATUS, &firmware_status, remaining));
        if (firmware_status == 3U) {
            return 0;
        }
        remaining = remaining_ms(deadline);
        if (remaining == 0U) {
            return ERR_TIMEOUT;
        }
        delay_ms(remaining < 1U ? remaining : 1U);
    }
}

static int set_timing_until(ch32_vl53l4cd_ctx_t *ctx, uint32_t timing_ms,
                            uint32_t deadline)
{
    uint16_t oscillator;
    uint16_t exponent = 0U;
    uint32_t budget;
    uint32_t macro_period;
    uint32_t value;
    uint32_t period;

    uint32_t remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(r16_timeout(ctx, REG_OSC_FREQUENCY, &oscillator, remaining));
    if (oscillator == 0U || timing_ms * 1000U <= 2500U) {
        return ERR_INVALID_PARAM;
    }
    budget = timing_ms * 1000U - 2500U;
    macro_period = (uint32_t)(
        ((uint64_t)2304U * (0x40000000ULL / oscillator)) >> 6U);
    budget <<= 12U;

    period = macro_period * 16U;
    value = ((budget + ((period >> 6U) >> 1U)) / (period >> 6U)) - 1U;
    while ((value & 0xFFFFFF00U) != 0U) {
        value >>= 1U;
        ++exponent;
    }
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(w16_timeout(ctx, REG_RANGE_CONFIG_A,
                    (uint16_t)((exponent << 8U) | (value & 0xFFU)),
                    remaining));

    exponent = 0U;
    period = macro_period * 12U;
    value = ((budget + ((period >> 6U) >> 1U)) / (period >> 6U)) - 1U;
    while ((value & 0xFFFFFF00U) != 0U) {
        value >>= 1U;
        ++exponent;
    }
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    return w16_timeout(ctx, REG_RANGE_CONFIG_B,
                       (uint16_t)((exponent << 8U) | (value & 0xFFU)),
                       remaining);
}

static int sensor_init(ch32_vl53l4cd_ctx_t *ctx)
{
    uint32_t deadline = now_ms() + CH32_VL53L4CD_INIT_TIMEOUT_MS;
    uint32_t remaining;
    uint16_t model_id;

    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(r16_timeout(ctx, REG_MODEL_ID, &model_id, remaining));
    if (model_id != CH32_VL53L4CD_MODEL_ID) {
        LOG_ERR("id_check FAIL expected=0x%04X got=0x%04X",
                CH32_VL53L4CD_MODEL_ID, model_id);
        return ERR_CH32_VL53L4CD_ID_MISMATCH;
    }
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(wait_firmware_ready(ctx, remaining < 1000U ? remaining : 1000U));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(write_data_timeout(ctx, 0x002DU, s_default_config,
                           sizeof(s_default_config), remaining));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    {
        uint8_t mux;
        TRY(r8_timeout(ctx, REG_GPIO_MUX, &mux, remaining));
        ctx->ready_level = ((mux >> 4U) & 1U) == 0U;
    }

#define INIT_W8(reg_, value_) do {                              \
        remaining = remaining_ms(deadline);                     \
        if (remaining == 0U) return ERR_TIMEOUT;                \
        TRY(w8_timeout(ctx, (reg_), (value_), remaining));      \
    } while (0)
    INIT_W8(REG_SYSTEM_START, 0x40U);
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(wait_ready(ctx, remaining < 1000U ? remaining : 1000U));
    INIT_W8(REG_INTERRUPT_CLEAR, 1U);
    INIT_W8(REG_SYSTEM_START, 0U);
    INIT_W8(REG_VHV_TIMEOUT, 9U);
    INIT_W8(0x000BU, 0U);
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(w16_timeout(ctx, 0x0024U, 0x0500U, remaining));
    TRY(set_timing_until(ctx, 50U, deadline));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(w32_timeout(ctx, REG_INTERMEASUREMENT, 0U, remaining));
    INIT_W8(REG_SYSTEM_START, 0x21U);
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(wait_ready(ctx, remaining < 1000U ? remaining : 1000U));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
#undef INIT_W8
    return w8_timeout(ctx, REG_INTERRUPT_CLEAR, 1U, remaining);
}
int ch32_vl53l4cd_init(ch32_vl53l4cd_handle_t *handle,
                       const ch32_vl53l4cd_cfg_t *cfg)
{
    ch32_vl53l4cd_ctx_t *ctx = NULL;
    ch32_i2c_multi_result_t gateway_result;
    bool found = false;
    int result;

    if (handle == NULL || cfg == NULL ||
        cfg->i2c_addr != CH32_VL53L4CD_I2C_ADDR_DEFAULT ||
        cfg->data_ready_timeout_ms == 0U ||
        valid_node(cfg->ch32_node) != 0) return ERR_INVALID_PARAM;
    *handle = NULL;
    for (size_t index = 0U; index < CH32_VL53L4CD_MAX_INSTANCES; ++index) {
        if (!s_instances[index].in_use) {
            ctx = &s_instances[index];
            break;
        }
    }
    if (ctx == NULL) return ERR_BUSY;
    memset(ctx, 0, sizeof(*ctx));
    ctx->in_use = true;
    ctx->node = cfg->ch32_node;
    ctx->addr = cfg->i2c_addr;
    ctx->ready_timeout_ms = cfg->data_ready_timeout_ms;

    gateway_result = ch32_i2c_multi_set_speed_400k(ctx->node);
    if (gateway_result == CH32_I2C_MULTI_RESULT_OK) {
        gateway_result = ch32_i2c_multi_probe(ctx->node, ctx->addr, &found);
    }
    if (gateway_result != CH32_I2C_MULTI_RESULT_OK || !found) {
        result = gateway_result == CH32_I2C_MULTI_RESULT_OK
                     ? ERR_NO_DEVICE : map_gateway(gateway_result);
        memset(ctx, 0, sizeof(*ctx));
        return result;
    }
    result = sensor_init(ctx);
    if (result != 0) {
        LOG_ERR("module init FAIL token=0x%04X node=%u err=%d",
                ctx->node->token, ctx->node->node_id, result);
        memset(ctx, 0, sizeof(*ctx));
        return result;
    }
    ctx->initialized = true;
    *handle = ctx;
    LOG_INF("init OK addr=0x29 id=0xEBAA token=0x%04X node=%u",
            ctx->node->token, ctx->node->node_id);
    return 0;
}
int ch32_vl53l4cd_deinit(ch32_vl53l4cd_handle_t handle)
{
    int result;

    if (handle == NULL || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    result = w8(handle, REG_SYSTEM_START, 0U);
    LOG_INF("deinit %s token=0x%04X node=%u",
            result == 0 ? "OK" : "FAIL", handle->node->token,
            handle->node->node_id);
    memset(handle, 0, sizeof(*handle));
    return result;
}
static int read_measurement_once(ch32_vl53l4cd_handle_t handle,
                                 ch32_vl53l4cd_result_t *out,
                                 uint32_t deadline)
{
    static const uint8_t status_map[24] = {
        255U, 255U, 255U, 5U, 2U, 4U, 1U, 7U,
        3U, 0U, 255U, 255U, 9U, 13U, 255U, 255U,
        255U, 255U, 10U, 6U, 255U, 255U, 11U, 12U,
    };
    uint8_t raw_status;
    uint16_t distance_mm;

    uint32_t remaining;

    TRY(wait_ready_until(handle, deadline));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(r8_timeout(handle, REG_RANGE_STATUS, &raw_status, remaining));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(r16_timeout(handle, REG_DISTANCE, &distance_mm, remaining));
    remaining = remaining_ms(deadline);
    if (remaining == 0U) return ERR_TIMEOUT;
    TRY(w8_timeout(handle, REG_INTERRUPT_CLEAR, 1U, remaining));
    raw_status &= 0x1FU;
    out->range_status = raw_status < 24U ? status_map[raw_status] : 255U;
    out->distance_mm = distance_mm;
    out->valid = out->range_status == 0U &&
                 distance_mm <= CH32_VL53L4CD_RANGE_MAX_MM;
    out->out_of_range = !out->valid;
    return out->valid ? 0 : ERR_CH32_VL53L4CD_OUT_OF_RANGE;
}

int ch32_vl53l4cd_read(ch32_vl53l4cd_handle_t handle,
                       ch32_vl53l4cd_result_t *out)
{
    int result = ERR_CH32_VL53L4CD_COMM;
    uint32_t deadline;

    if (handle == NULL || out == NULL) return ERR_INVALID_PARAM;
    if (!handle->initialized) return ERR_NOT_INIT;
    deadline = now_ms() + handle->ready_timeout_ms;
    for (uint32_t attempt = 0U;
         attempt < CH32_VL53L4CD_READ_MAX_ATTEMPTS;
         ++attempt) {
        memset(out, 0, sizeof(*out));
        if (remaining_ms(deadline) == 0U) return ERR_TIMEOUT;
        result = read_measurement_once(handle, out, deadline);
        if (result == 0 || result == ERR_CH32_VL53L4CD_OUT_OF_RANGE) return result;
    }
    return result;
}
