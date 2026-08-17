#include "vl53l4cd.h"
#include <stdio.h>
#include <string.h>
#include "bsp_i2c.h"
#include "esp_err.h"
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
#define LOG_INF(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOG_ERR(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define TRY(x) do { int r_=(x); if(r_!=0)return r_; } while(0)
typedef struct vl53l4cd_ctx {
    bool allocated;
    bool initialized;
    uint8_t ready_level;
    i2c_master_dev_handle_t dev;
    vl53l4cd_cfg_t cfg;
} vl53l4cd_ctx_t;
static vl53l4cd_ctx_t s_instance;
/* ST VL53L4CD ULD V1.0.0 default configuration, registers 0x002D..0x0087. */
static const uint8_t s_default_config[] = {
0x12,0x00,0x00,0x11,0x02,0x00,0x02,0x08,0x00,0x08,0x10,0x01,0x01,0x00,0x00,0x00,
0x00,0xFF,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x20,0x0B,0x00,0x00,0x02,0x14,0x21,
0x00,0x00,0x05,0x00,0x00,0x00,0x00,0xC8,0x00,0x00,0x38,0xFF,0x01,0x00,0x08,0x00,
0x00,0x01,0xCC,0x07,0x01,0xF1,0x05,0x00,0xA0,0x00,0x80,0x08,0x38,0x00,0x00,0x00,
0x00,0x0F,0x89,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x07,0x05,0x06,0x06,0x00,
0x00,0x02,0xC7,0xFF,0x9B,0x00,0x00,0x00,0x01,0x00,0x00};
static void delay_ms(uint32_t ms){TickType_t t=pdMS_TO_TICKS(ms);vTaskDelay(t?t:1U);}
static int map_err(esp_err_t error)
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
    return ERR_VL53L4CD_COMM;
}
static uint32_t remaining_timeout_ms(vl53l4cd_ctx_t *ctx, int64_t deadline_us)
{
    int64_t remaining_us = deadline_us - esp_timer_get_time();
    uint32_t timeout_ms;

    if (remaining_us <= 0) {
        return 0U;
    }
    timeout_ms = (uint32_t)((remaining_us + 999LL) / 1000LL);
    return timeout_ms < ctx->cfg.timeout_ms ? timeout_ms : ctx->cfg.timeout_ms;
}

static int write_data_timeout(vl53l4cd_ctx_t *ctx,
                              uint16_t reg,
                              const uint8_t *data,
                              size_t length,
                              uint32_t timeout_ms)
{
    uint8_t buffer[32];
    int64_t deadline_us;

    if (timeout_ms == 0U) {
        return ERR_TIMEOUT;
    }
    deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000LL;

    while (length > 0U) {
        size_t chunk_length = length > 30U ? 30U : length;
        uint32_t remaining_ms = remaining_timeout_ms(ctx, deadline_us);
        esp_err_t error;

        if (remaining_ms == 0U) {
            return ERR_TIMEOUT;
        }

        buffer[0] = (uint8_t)(reg >> 8U);
        buffer[1] = (uint8_t)reg;
        memcpy(&buffer[2], data, chunk_length);
        error = bsp_i2c_write(ctx->dev, buffer, chunk_length + 2U,
                              (int)remaining_ms);
        if (error != ESP_OK) {
            LOG_ERR("i2c write FAIL reg=0x%04X err=%s",
                    reg, esp_err_to_name(error));
            return map_err(error);
        }

        reg = (uint16_t)(reg + chunk_length);
        data += chunk_length;
        length -= chunk_length;
    }
    return 0;
}

static int write_data(vl53l4cd_ctx_t *ctx, uint16_t reg,
                      const uint8_t *data, size_t length)
{
    return write_data_timeout(ctx, reg, data, length, ctx->cfg.timeout_ms);
}

static int read_data_timeout(vl53l4cd_ctx_t *ctx,
                             uint16_t reg,
                             uint8_t *data,
                             size_t length,
                             uint32_t timeout_ms)
{
    uint8_t index[2] = {(uint8_t)(reg >> 8U), (uint8_t)reg};
    esp_err_t error = bsp_i2c_write_read(ctx->dev, index, sizeof(index),
                                         data, length,
                                         (int)timeout_ms);

    if (error != ESP_OK) {
        LOG_ERR("i2c read FAIL reg=0x%04X err=%s",
                reg, esp_err_to_name(error));
    }
    return map_err(error);
}

static int w8(vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t value)
{
    return write_data(ctx, reg, &value, 1U);
}

static int w8_timeout(vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t value,
                      uint32_t timeout_ms)
{
    return write_data_timeout(ctx, reg, &value, 1U, timeout_ms);
}

static int w16_timeout(vl53l4cd_ctx_t *ctx, uint16_t reg, uint16_t value,
                       uint32_t timeout_ms)
{
    uint8_t data[2] = {(uint8_t)(value >> 8U), (uint8_t)value};
    return write_data_timeout(ctx, reg, data, sizeof(data), timeout_ms);
}

static int w32_timeout(vl53l4cd_ctx_t *ctx, uint16_t reg, uint32_t value,
                       uint32_t timeout_ms)
{
    uint8_t data[4] = {
        (uint8_t)(value >> 24U), (uint8_t)(value >> 16U),
        (uint8_t)(value >> 8U), (uint8_t)value,
    };
    return write_data_timeout(ctx, reg, data, sizeof(data), timeout_ms);
}

static int r8_timeout(vl53l4cd_ctx_t *ctx, uint16_t reg, uint8_t *value,
                      uint32_t timeout_ms)
{
    return read_data_timeout(ctx, reg, value, 1U, timeout_ms);
}

static int r16_timeout(vl53l4cd_ctx_t *ctx, uint16_t reg, uint16_t *value,
                       uint32_t timeout_ms)
{
    uint8_t data[2];

    TRY(read_data_timeout(ctx, reg, data, sizeof(data), timeout_ms));
    *value = ((uint16_t)data[0] << 8U) | data[1];
    return 0;
}

static int data_ready_timeout(vl53l4cd_ctx_t *ctx, bool *ready,
                              uint32_t timeout_ms)
{
    uint8_t gpio_status;

    TRY(r8_timeout(ctx, REG_GPIO_STATUS, &gpio_status, timeout_ms));
    *ready = (gpio_status & 1U) == ctx->ready_level;
    return 0;
}

static int wait_ready_until(vl53l4cd_ctx_t *ctx, int64_t deadline_us)
{
    while (true) {
        bool ready;
        uint32_t timeout_ms = remaining_timeout_ms(ctx, deadline_us);
        int result;

        if (timeout_ms == 0U) {
            return ERR_VL53L4CD_DATA_NOT_READY;
        }
        result = data_ready_timeout(ctx, &ready, timeout_ms);
        if (result != 0) {
            return result == ERR_TIMEOUT && esp_timer_get_time() >= deadline_us
                       ? ERR_VL53L4CD_DATA_NOT_READY
                       : result;
        }
        if (ready) {
            return 0;
        }
        if (esp_timer_get_time() >= deadline_us) {
            return ERR_VL53L4CD_DATA_NOT_READY;
        }
        timeout_ms = remaining_timeout_ms(ctx, deadline_us);
        if (timeout_ms == 0U) {
            return ERR_VL53L4CD_DATA_NOT_READY;
        }
        delay_ms(timeout_ms < POLL_MS ? timeout_ms : POLL_MS);
    }
}

static int set_range_timing_until(vl53l4cd_ctx_t *ctx, uint32_t timing_ms,
                                  int64_t deadline_us)
{
    uint16_t oscillator_frequency;
    uint16_t encoded_timeout;
    uint16_t exponent = 0U;
    uint32_t timing_budget_us;
    uint32_t macro_period_us;
    uint32_t encoded_value;
    uint32_t scaled_macro_period;

    uint32_t timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(r16_timeout(ctx, REG_OSC_FREQUENCY, &oscillator_frequency,
                    timeout_ms));
    if (oscillator_frequency == 0U || timing_ms < 10U || timing_ms > 200U) {
        return ERR_INVALID_PARAM;
    }

    timing_budget_us = timing_ms * 1000U - 2500U;
    macro_period_us = (uint32_t)(
        ((uint64_t)2304U * (0x40000000ULL / oscillator_frequency)) >> 6U);
    timing_budget_us <<= 12U;

    scaled_macro_period = macro_period_us * 16U;
    encoded_value =
        ((timing_budget_us + ((scaled_macro_period >> 6U) >> 1U)) /
         (scaled_macro_period >> 6U)) - 1U;
    while ((encoded_value & 0xFFFFFF00U) != 0U) {
        encoded_value >>= 1U;
        exponent++;
    }
    encoded_timeout = (uint16_t)((exponent << 8U) |
                                 (encoded_value & 0xFFU));
    timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(w16_timeout(ctx, REG_RANGE_CONFIG_A, encoded_timeout, timeout_ms));

    exponent = 0U;
    scaled_macro_period = macro_period_us * 12U;
    encoded_value =
        ((timing_budget_us + ((scaled_macro_period >> 6U) >> 1U)) /
         (scaled_macro_period >> 6U)) - 1U;
    while ((encoded_value & 0xFFFFFF00U) != 0U) {
        encoded_value >>= 1U;
        exponent++;
    }
    encoded_timeout = (uint16_t)((exponent << 8U) |
                                 (encoded_value & 0xFFU));
    timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    return w16_timeout(ctx, REG_RANGE_CONFIG_B, encoded_timeout, timeout_ms);
}
static int sensor_init(vl53l4cd_ctx_t *ctx)
{
    int64_t init_deadline_us = esp_timer_get_time() +
                               (int64_t)VL53L4CD_INIT_TIMEOUT_MS * 1000LL;
    int64_t firmware_deadline_us;
    int64_t ready_deadline_us;
    uint32_t timeout_ms;
    uint16_t model_id;
    uint8_t firmware_status;
    uint8_t gpio_mux;

    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(r16_timeout(ctx, REG_MODEL_ID, &model_id, timeout_ms));
    if (model_id != VL53L4CD_MODEL_ID) {
        LOG_ERR("id_check FAIL expected=0x%04X got=0x%04X",
                VL53L4CD_MODEL_ID, model_id);
        return ERR_VL53L4CD_ID_MISMATCH;
    }

    firmware_deadline_us = esp_timer_get_time() + 1000000LL;
    if (firmware_deadline_us > init_deadline_us) {
        firmware_deadline_us = init_deadline_us;
    }
    while (true) {
        uint32_t timeout_ms = remaining_timeout_ms(ctx, firmware_deadline_us);

        if (timeout_ms == 0U) {
            return ERR_TIMEOUT;
        }
        TRY(r8_timeout(ctx, REG_FIRMWARE_STATUS, &firmware_status, timeout_ms));
        if (firmware_status == 3U) {
            break;
        }
        if (esp_timer_get_time() >= firmware_deadline_us) {
            return ERR_TIMEOUT;
        }
        delay_ms(1U);
    }

    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(write_data_timeout(ctx, 0x002DU, s_default_config,
                           sizeof(s_default_config), timeout_ms));
    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(r8_timeout(ctx, REG_GPIO_MUX, &gpio_mux, timeout_ms));
    ctx->ready_level = ((gpio_mux >> 4U) & 1U) != 0U ? 0U : 1U;
    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(w8_timeout(ctx, REG_SYSTEM_START, 0x40U, timeout_ms));
    ready_deadline_us = esp_timer_get_time() + 1000000LL;
    if (ready_deadline_us > init_deadline_us) ready_deadline_us = init_deadline_us;
    TRY(wait_ready_until(ctx, ready_deadline_us));

#define INIT_W8(reg_, value_) do {                                      \
        timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);       \
        if (timeout_ms == 0U) return ERR_TIMEOUT;                       \
        TRY(w8_timeout(ctx, (reg_), (value_), timeout_ms));             \
    } while (0)
    INIT_W8(REG_INTERRUPT_CLEAR, 1U);
    INIT_W8(REG_SYSTEM_START, 0U);
    INIT_W8(REG_VHV_TIMEOUT, 0x09U);
    INIT_W8(0x000BU, 0U);
    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(w16_timeout(ctx, 0x0024U, 0x0500U, timeout_ms));
    TRY(set_range_timing_until(ctx, 50U, init_deadline_us));
    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
    TRY(w32_timeout(ctx, REG_INTERMEASUREMENT, 0U, timeout_ms));
    INIT_W8(REG_SYSTEM_START, 0x21U);
    ready_deadline_us = esp_timer_get_time() + 1000000LL;
    if (ready_deadline_us > init_deadline_us) ready_deadline_us = init_deadline_us;
    TRY(wait_ready_until(ctx, ready_deadline_us));
    timeout_ms = remaining_timeout_ms(ctx, init_deadline_us);
    if (timeout_ms == 0U) return ERR_TIMEOUT;
#undef INIT_W8
    return w8_timeout(ctx, REG_INTERRUPT_CLEAR, 1U, timeout_ms);
}
int vl53l4cd_init(vl53l4cd_handle_t *handle, const vl53l4cd_cfg_t *cfg)
{
    esp_err_t error = ESP_OK;
    int result;

    if (handle == NULL || cfg == NULL || s_instance.allocated ||
        cfg->i2c_addr != VL53L4CD_I2C_ADDR_DEFAULT ||
        cfg->bus_speed_hz == 0U ||
        cfg->bus_speed_hz > VL53L4CD_BUS_SPEED_HZ ||
        cfg->timeout_ms == 0U || cfg->data_ready_timeout_ms == 0U) {
        return ERR_INVALID_PARAM;
    }

    *handle = NULL;
    memset(&s_instance, 0, sizeof(s_instance));
    s_instance.allocated = true;
    s_instance.cfg = *cfg;

    error = bsp_i2c_add_device_7bit(cfg->i2c_addr, cfg->bus_speed_hz,
                                    &s_instance.dev);
    if (error != ESP_OK) {
        memset(&s_instance, 0, sizeof(s_instance));
        return map_err(error);
    }

    result = sensor_init(&s_instance);
    if (result != 0) {
        bsp_i2c_remove_device(s_instance.dev);
        memset(&s_instance, 0, sizeof(s_instance));
        LOG_ERR("init FAIL err=%d", result);
        return result;
    }

    s_instance.initialized = true;
    *handle = &s_instance;
    LOG_INF("init OK addr=0x29 id=0xEBAA range_max=1300mm");
    return 0;
}

int vl53l4cd_deinit(vl53l4cd_handle_t handle)
{
    int result;

    if (handle != &s_instance || !s_instance.initialized) {
        return ERR_NOT_INIT;
    }

    result = w8(&s_instance, REG_SYSTEM_START, 0U);
    bsp_i2c_remove_device(s_instance.dev);
    memset(&s_instance, 0, sizeof(s_instance));
    LOG_INF("deinit %s", result == 0 ? "OK" : "FAIL");
    return result;
}
static int read_measurement_once(vl53l4cd_ctx_t *ctx,
                                 vl53l4cd_result_t *result,
                                 int64_t deadline_us)
{
    static const uint8_t status_map[24] = {
        255U, 255U, 255U, 5U, 2U, 4U, 1U, 7U,
        3U, 0U, 255U, 255U, 9U, 13U, 255U, 255U,
        255U, 255U, 10U, 6U, 255U, 255U, 11U, 12U,
    };
    uint8_t raw_status;
    uint16_t distance_mm;
    uint32_t timeout_ms;

    TRY(wait_ready_until(ctx, deadline_us));
    timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) {
        return ERR_TIMEOUT;
    }
    TRY(r8_timeout(ctx, REG_RANGE_STATUS, &raw_status, timeout_ms));
    timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) {
        return ERR_TIMEOUT;
    }
    TRY(r16_timeout(ctx, REG_DISTANCE, &distance_mm, timeout_ms));
    timeout_ms = remaining_timeout_ms(ctx, deadline_us);
    if (timeout_ms == 0U) {
        return ERR_TIMEOUT;
    }
    TRY(w8_timeout(ctx, REG_INTERRUPT_CLEAR, 1U, timeout_ms));

    raw_status &= 0x1FU;
    result->range_status = raw_status < 24U ? status_map[raw_status] : 255U;
    result->distance_mm = distance_mm;
    result->valid = result->range_status == 0U &&
                    distance_mm <= VL53L4CD_RANGE_MAX_MM;
    result->out_of_range = !result->valid;
    return result->valid ? 0 : ERR_VL53L4CD_OUT_OF_RANGE;
}

int vl53l4cd_read(vl53l4cd_handle_t handle, vl53l4cd_result_t *result)
{
    int64_t deadline_us;
    int read_result = ERR_VL53L4CD_COMM;

    if (handle != &s_instance || result == NULL) {
        return ERR_INVALID_PARAM;
    }
    if (!s_instance.initialized) {
        return ERR_NOT_INIT;
    }
    deadline_us = esp_timer_get_time() +
                  (int64_t)s_instance.cfg.data_ready_timeout_ms * 1000LL;

    /* 默认只重试完整测距一次；越界是完成的结果，不重试。 */
    for (uint32_t attempt = 0U;
         attempt < VL53L4CD_READ_MAX_ATTEMPTS;
         ++attempt) {
        memset(result, 0, sizeof(*result));
        read_result = read_measurement_once(&s_instance, result, deadline_us);
        if (read_result == 0 || read_result == ERR_VL53L4CD_OUT_OF_RANGE) {
            return read_result;
        }
    }

    return read_result;
}
