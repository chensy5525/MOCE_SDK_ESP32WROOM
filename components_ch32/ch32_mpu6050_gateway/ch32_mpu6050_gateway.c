/** MPU-6050 CH32-CAN-I2C bridge attitude driver.
 * Registers: InvenSense RM-MPU-6000A-00 Rev.4.2.
 * Discovery owns the stable node; this component never accesses CAN directly.
 */
#include "mpu6050.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MPU6050"
#define LOGI(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGW(f, ...) printf("[WRN][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGE(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define RETRIES 3U
#define RETRY_MS 2U
#define RESET_MS 100U
#define SAMPLE_MS (1000U / MPU6050_SAMPLE_RATE_HZ)
#define BURST_LEN 14U
#define WHO_VALUE 0x68U
#define REG_SMPLRT_DIV 0x19U
#define REG_CONFIG 0x1AU
#define REG_GYRO_CONFIG 0x1BU
#define REG_ACCEL_CONFIG 0x1CU
#define REG_ACCEL_XOUT_H 0x3BU
#define REG_PWR_MGMT_1 0x6BU
#define REG_PWR_MGMT_2 0x6CU
#define REG_WHO_AM_I 0x75U
#define ACCEL_LSB_G 16384.0f
#define GYRO_LSB_DPS 131.0f
#define RAD_DEG 57.29577951308232f
#define CAL_MIN 20U

typedef struct {
    float ax, ay, az, gx, gy, gz;
} sample_t;

struct mpu6050_ctx {
    ch32_i2c_multi_node_t *node;
    uint8_t addr;
    uint32_t timeout_ms;
    float alpha, bx, by, bz, roll, pitch, yaw;
    int64_t last_us;
    uint32_t samples, ok_count, error_count;
    bool in_use, initialized, seeded;
};

static struct mpu6050_ctx s_ctx[MPU6050_MAX_BRIDGE_INSTANCES];

static bool node_valid(const ch32_i2c_multi_node_t *node)
{
    return node != NULL && node->ready && node->token != 0U &&
           node->device_type == CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C &&
           node->node_id >= 0x01U && node->node_id <= 0x20U;
}

static int map_result(ch32_i2c_multi_result_t result)
{
    if (result == CH32_I2C_MULTI_RESULT_OK) return 0;
    if (result == CH32_I2C_MULTI_RESULT_TIMEOUT ||
        result == CH32_I2C_MULTI_RESULT_NO_DATA) return ERR_TIMEOUT;
    if (result == CH32_I2C_MULTI_RESULT_INVALID_ARG) return ERR_INVALID_PARAM;
    if (result == CH32_I2C_MULTI_RESULT_BUSY) return ERR_BUSY;
    if (result == CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND) return ERR_NO_DEVICE;
    return ERR_HW_FAULT;
}

static int16_t be_i16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8U) | p[1]);
}

static float wrap(float degrees)
{
    while (degrees >= 180.0f) degrees -= 360.0f;
    while (degrees < -180.0f) degrees += 360.0f;
    return degrees;
}

static int write_reg(mpu6050_handle_t h, uint8_t reg, uint8_t value)
{
    ch32_i2c_multi_result_t gr = CH32_I2C_MULTI_RESULT_COMM_FAIL;
    if (!node_valid(h->node)) return ERR_NO_DEVICE;
    for (uint32_t attempt = 1U; attempt <= RETRIES; ++attempt) {
        gr = ch32_i2c_multi_write_reg_to(h->node, h->addr, reg, &value, 1U);
        if (gr == CH32_I2C_MULTI_RESULT_OK) {
            h->ok_count++;
            return 0;
        }
        h->error_count++;
        LOGW("downstream write retry=%lu token=0x%04X node=%u reg=0x%02X result=%s",
             (unsigned long)attempt, h->node->token, h->node->node_id, reg,
             ch32_i2c_multi_result_text(gr));
        if (attempt < RETRIES) vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
    }
    return map_result(gr);
}

static int read_regs(mpu6050_handle_t h, uint8_t reg, uint8_t *data, uint8_t len)
{
    ch32_i2c_multi_result_t gr = CH32_I2C_MULTI_RESULT_COMM_FAIL;
    if (data == NULL || len == 0U) return ERR_INVALID_PARAM;
    if (!node_valid(h->node)) return ERR_NO_DEVICE;
    for (uint32_t attempt = 1U; attempt <= RETRIES; ++attempt) {
        /* Gateway uses a nonzero request_id and waits for all chunks + DONE. */
        gr = ch32_i2c_multi_read_regs_from(h->node, h->addr, reg, data, len);
        if (gr == CH32_I2C_MULTI_RESULT_OK) {
            h->ok_count++;
            return 0;
        }
        h->error_count++;
        LOGW("downstream read retry=%lu token=0x%04X node=%u reg=0x%02X result=%s",
             (unsigned long)attempt, h->node->token, h->node->node_id, reg,
             ch32_i2c_multi_result_text(gr));
        if (attempt < RETRIES) vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
    }
    return map_result(gr);
}

static int read_sample(mpu6050_handle_t h, sample_t *s)
{
    uint8_t data[BURST_LEN];
    int result = read_regs(h, REG_ACCEL_XOUT_H, data, sizeof(data));
    if (result != 0) return result;
    s->ax = (float)be_i16(&data[0]) / ACCEL_LSB_G;
    s->ay = (float)be_i16(&data[2]) / ACCEL_LSB_G;
    s->az = (float)be_i16(&data[4]) / ACCEL_LSB_G;
    s->gx = (float)be_i16(&data[8]) / GYRO_LSB_DPS;
    s->gy = (float)be_i16(&data[10]) / GYRO_LSB_DPS;
    s->gz = (float)be_i16(&data[12]) / GYRO_LSB_DPS;
    return 0;
}

static int configure(mpu6050_handle_t h)
{
    uint8_t who = 0U;
    int result = write_reg(h, REG_PWR_MGMT_1, 0x80U);
    if (result != 0) return result;
    vTaskDelay(pdMS_TO_TICKS(RESET_MS));
    result = read_regs(h, REG_WHO_AM_I, &who, 1U);
    if (result != 0) return result;
    if (who != WHO_VALUE) {
        LOGE("init FAIL, stage=identify expected=0x%02X got=0x%02X", WHO_VALUE, who);
        return ERR_MPU6050_ID_MISMATCH;
    }
    if ((result = write_reg(h, REG_PWR_MGMT_1, 0x01U)) != 0) return result;
    if ((result = write_reg(h, REG_PWR_MGMT_2, 0x00U)) != 0) return result;
    if ((result = write_reg(h, REG_CONFIG, 0x03U)) != 0) return result;
    if ((result = write_reg(h, REG_SMPLRT_DIV, 9U)) != 0) return result;
    if ((result = write_reg(h, REG_GYRO_CONFIG, 0x00U)) != 0) return result;
    return write_reg(h, REG_ACCEL_CONFIG, 0x00U);
}

static int calibrate(mpu6050_handle_t h, uint16_t count)
{
    sample_t s;
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    uint16_t valid = 0U;
    for (uint16_t i = 0U; i < count; ++i) {
        if (read_sample(h, &s) == 0) {
            sx += s.gx; sy += s.gy; sz += s.gz; valid++;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
    if (valid < CAL_MIN || valid < count / 2U) return ERR_MPU6050_CALIBRATION;
    h->bx = sx / valid; h->by = sy / valid; h->bz = sz / valid;
    LOGI("calibration OK, token=0x%04X samples=%u", h->node->token, valid);
    return 0;
}

int mpu6050_init_device(mpu6050_handle_t *handle, const mpu6050_cfg_t *cfg)
{
    mpu6050_handle_t h = NULL;
    bool found = false;
    int result;
    if (handle == NULL || cfg == NULL) return ERR_INVALID_PARAM;
    *handle = NULL;
    if (!node_valid(cfg->ch32_node) || cfg->i2c_addr != MPU6050_I2C_ADDR ||
        cfg->clk_speed_hz != MPU6050_I2C_FREQ_HZ || cfg->bridge_timeout_ms == 0U ||
        cfg->calibration_samples < CAL_MIN || cfg->complementary_alpha < 0.0f ||
        cfg->complementary_alpha > 1.0f) {
        LOGE("init FAIL, stage=module reason=invalid_config");
        return ERR_INVALID_PARAM;
    }
    for (size_t i = 0U; i < MPU6050_MAX_BRIDGE_INSTANCES; ++i) {
        if (!s_ctx[i].in_use) { h = &s_ctx[i]; break; }
    }
    if (h == NULL) return ERR_BUSY;
    memset(h, 0, sizeof(*h));
    h->in_use = true; h->node = cfg->ch32_node; h->addr = cfg->i2c_addr;
    h->timeout_ms = cfg->bridge_timeout_ms; h->alpha = cfg->complementary_alpha;

    if (ch32_i2c_multi_set_speed_400k(h->node) != CH32_I2C_MULTI_RESULT_OK) {
        result = ERR_NO_DEVICE;
        LOGE("init FAIL, stage=downstream_speed token=0x%04X", h->node->token);
        goto fail;
    }
    if (ch32_i2c_multi_probe(h->node, h->addr, &found) != CH32_I2C_MULTI_RESULT_OK || !found) {
        result = ERR_NO_DEVICE;
        LOGE("init FAIL, stage=downstream_probe token=0x%04X addr=0x%02X",
             h->node->token, h->addr);
        goto fail;
    }
    if ((result = configure(h)) != 0) {
        LOGE("init FAIL, stage=module_config token=0x%04X err=%d", h->node->token, result);
        goto fail;
    }
    if ((result = calibrate(h, cfg->calibration_samples)) != 0) {
        LOGE("init FAIL, stage=calibration token=0x%04X err=%d", h->node->token, result);
        goto fail;
    }
    h->initialized = true; h->last_us = esp_timer_get_time(); *handle = h;
    LOGI("init OK, token=0x%04X node=%u addr=0x%02X rate=%uHz",
         h->node->token, h->node->node_id, h->addr, MPU6050_SAMPLE_RATE_HZ);
    return 0;
fail:
    memset(h, 0, sizeof(*h));
    return result;
}

int mpu6050_deinit_device(mpu6050_handle_t h)
{
    if (h == NULL || !h->in_use || !h->initialized) return ERR_NOT_INIT;
    LOGI("deinit OK, token=0x%04X", h->node->token);
    memset(h, 0, sizeof(*h));
    return 0;
}

int mpu6050_read_orientation(mpu6050_handle_t h, mpu6050_orientation_t *o)
{
    sample_t s;
    float norm, ar, ap, dt, gr, gp;
    int64_t now;
    int result;
    if (h == NULL || !h->in_use || !h->initialized) return ERR_NOT_INIT;
    if (o == NULL) return ERR_INVALID_PARAM;
    memset(o, 0, sizeof(*o));
    if (!node_valid(h->node)) return ERR_NO_DEVICE;
    if ((result = read_sample(h, &s)) != 0) {
        LOGE("run FAIL, stage=downstream_read token=0x%04X node=%u err=%d",
             h->node->token, h->node->node_id, result);
        return result;
    }
    norm = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    if (!isfinite(norm) || norm < 0.5f || norm > 1.5f) return ERR_MPU6050_DATA_INVALID;
    ar = atan2f(s.ay, s.az) * RAD_DEG;
    ap = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * RAD_DEG;
    now = esp_timer_get_time(); dt = (float)(now - h->last_us) / 1000000.0f; h->last_us = now;
    if (!h->seeded || dt <= 0.0f || dt > 0.1f) {
        h->roll = ar; h->pitch = ap; h->seeded = true;
    } else {
        gr = h->roll + (s.gx - h->bx) * dt;
        gp = h->pitch + (s.gy - h->by) * dt;
        h->roll = h->alpha * gr + (1.0f - h->alpha) * ar;
        h->pitch = h->alpha * gp + (1.0f - h->alpha) * ap;
        h->yaw = wrap(h->yaw + (s.gz - h->bz) * dt);
    }
    h->samples++;
    o->roll_deg = wrap(h->roll); o->pitch_deg = wrap(h->pitch); o->yaw_deg = h->yaw;
    o->valid = true; o->sample_count = h->samples;
    return 0;
}
