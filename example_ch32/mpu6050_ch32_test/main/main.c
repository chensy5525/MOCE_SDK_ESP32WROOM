#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32_i2c_multi_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpu6050.h"

#define TAG "MPU6050_EXAMPLE"
#define LOGI(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGW(f, ...) printf("[WRN][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOGE(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define MAX_NODES 6U
#define INITIAL_DISCOVERY_MS 5000U
#define REDISCOVERY_TIMEOUT_MS 1000U
#define REDISCOVERY_PERIOD_MS 5000U
#define CALCULATION_MS 10U
#define PRINT_DIVIDER 20U
#define TEST_PRINTS 100U

typedef struct {
    ch32_i2c_multi_node_t *node;
    mpu6050_handle_t imu;
    uint32_t calculations;
    uint32_t prints;
} imu_slot_t;

static ch32_i2c_multi_node_t s_nodes[MAX_NODES];
static size_t s_node_count;
static imu_slot_t s_slots[MAX_NODES];

static ch32_i2c_multi_node_t *find_node(uint8_t type, uint16_t token)
{
    for (size_t i = 0U; i < s_node_count; ++i) {
        if (s_nodes[i].device_type == type && s_nodes[i].token == token) return &s_nodes[i];
    }
    return NULL;
}

static ch32_i2c_multi_node_t *merge_node(const ch32_i2c_multi_node_t *fresh)
{
    ch32_i2c_multi_node_t *stable;
    uint8_t old_id;
    if (fresh == NULL || !fresh->ready || fresh->token == 0U ||
        fresh->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C) return NULL;
    stable = find_node(fresh->device_type, fresh->token);
    if (stable != NULL) {
        old_id = stable->node_id;
        *stable = *fresh;
        if (old_id != stable->node_id) {
            LOGI("F2 node update token=0x%04X old=%u new=%u",
                 stable->token, old_id, stable->node_id);
        }
        return stable;
    }
    if (s_node_count >= MAX_NODES) {
        LOGE("F0/F1/F2 stable table full token=0x%04X", fresh->token);
        return NULL;
    }
    stable = &s_nodes[s_node_count++];
    *stable = *fresh;
    LOGI("F2 confirmed token=0x%04X node=%u fw=%u",
         stable->token, stable->node_id, stable->fw_version);
    return stable;
}

static bool has_addr(const ch32_i2c_multi_node_t *node, uint8_t addr)
{
    for (uint8_t i = 0U; node != NULL && i < node->i2c_addr_count; ++i) {
        if (node->i2c_addrs[i] == addr) return true;
    }
    return false;
}

static imu_slot_t *slot_for(ch32_i2c_multi_node_t *node)
{
    imu_slot_t *free_slot = NULL;
    for (size_t i = 0U; i < MAX_NODES; ++i) {
        if (s_slots[i].node == node) return &s_slots[i];
        if (free_slot == NULL && s_slots[i].node == NULL) free_slot = &s_slots[i];
    }
    if (free_slot != NULL) free_slot->node = node;
    return free_slot;
}

static void start_imu(ch32_i2c_multi_node_t *node)
{
    imu_slot_t *slot = slot_for(node);
    const mpu6050_cfg_t cfg = {
        .i2c_addr = MPU6050_I2C_ADDR,
        .clk_speed_hz = MPU6050_I2C_FREQ_HZ,
        .bridge_timeout_ms = MPU6050_BRIDGE_TIMEOUT_MS,
        .calibration_samples = MPU6050_CALIBRATION_SAMPLES,
        .complementary_alpha = MPU6050_COMPLEMENTARY_ALPHA_DEFAULT,
        .ch32_node = node,
    };
    int result;
    if (slot == NULL || slot->imu != NULL) return;
    LOGI("module calibration start token=0x%04X; keep stationary", node->token);
    result = mpu6050_init_device(&slot->imu, &cfg);
    if (result != 0) {
        LOGE("module init failed token=0x%04X node=%u err=%d",
             node->token, node->node_id, result);
        slot->node = NULL;
    }
}

static void discover(uint32_t timeout_ms)
{
    ch32_i2c_multi_node_t fresh[MAX_NODES] = {0};
    size_t count = 0U;
    ch32_i2c_multi_result_t gr = ch32_i2c_multi_discover_incremental(
        fresh, MAX_NODES, &count, timeout_ms);
    LOGI("F0/F1/F2 discovery result=%s fresh=%u stable=%u",
         ch32_i2c_multi_result_text(gr), (unsigned)count, (unsigned)s_node_count);
    if (gr != CH32_I2C_MULTI_RESULT_OK && count == 0U) {
        LOGW("F0/F1/F2 discovery failed; keeping stable table");
        return;
    }
    for (size_t i = 0U; i < count; ++i) {
        ch32_i2c_multi_node_t *node = merge_node(&fresh[i]);
        bool found = false;
        if (node == NULL) continue;
        found = has_addr(node, MPU6050_I2C_ADDR);
        if (!found) {
            gr = ch32_i2c_multi_probe(node, MPU6050_I2C_ADDR, &found);
            if (gr != CH32_I2C_MULTI_RESULT_OK) {
                LOGE("downstream scan/probe failed token=0x%04X node=%u result=%s",
                     node->token, node->node_id, ch32_i2c_multi_result_text(gr));
                continue;
            }
            if (found && node->i2c_addr_count < CH32_I2C_MULTI_MAX_ADDRS_PER_NODE) {
                node->i2c_addrs[node->i2c_addr_count++] = MPU6050_I2C_ADDR;
            }
        }
        if (found) {
            LOGI("downstream scan found addr=0x%02X token=0x%04X node=%u",
                 MPU6050_I2C_ADDR, node->token, node->node_id);
            start_imu(node);
        } else {
            LOGW("downstream scan no MPU6050 token=0x%04X node=%u",
                 node->token, node->node_id);
        }
    }
}

static bool tests_complete(void)
{
    bool any = false;
    for (size_t i = 0U; i < MAX_NODES; ++i) {
        if (s_slots[i].imu != NULL) {
            any = true;
            if (s_slots[i].prints < TEST_PRINTS) return false;
        }
    }
    return any;
}

void app_main(void)
{
    ch32_i2c_multi_config_t cfg;
    TickType_t last_wake, last_discovery;
    ch32_i2c_multi_default_config(&cfg);
    cfg.discovery_timeout_ms = INITIAL_DISCOVERY_MS;
    cfg.command_timeout_ms = MPU6050_BRIDGE_TIMEOUT_MS;
    if (ch32_i2c_multi_init(&cfg) != 0) {
        LOGE("CAN core init failed");
        return;
    }
    LOGI("CAN core init OK, bitrate=%u", CH32_CAN_GATEWAY_BITRATE_HZ);
    discover(INITIAL_DISCOVERY_MS);
    last_wake = xTaskGetTickCount();
    last_discovery = last_wake;
    while (!tests_complete()) {
        for (size_t i = 0U; i < MAX_NODES; ++i) {
            imu_slot_t *slot = &s_slots[i];
            mpu6050_orientation_t o;
            int result;
            if (slot->imu == NULL || slot->prints >= TEST_PRINTS) continue;
            result = mpu6050_read_orientation(slot->imu, &o);
            if (result != 0) {
                LOGE("module run failed token=0x%04X node=%u err=%d",
                     slot->node->token, slot->node->node_id, result);
                continue;
            }
            slot->calculations++;
            if (slot->calculations % PRINT_DIVIDER == 0U) {
                LOGI("token=0x%04X roll=%.2fdeg pitch=%.2fdeg yaw=%.2fdeg valid=%u",
                     slot->node->token, o.roll_deg, o.pitch_deg, o.yaw_deg,
                     o.valid ? 1U : 0U);
                slot->prints++;
            }
        }
        if (xTaskGetTickCount() - last_discovery >= pdMS_TO_TICKS(REDISCOVERY_PERIOD_MS)) {
            discover(REDISCOVERY_TIMEOUT_MS);
            last_discovery = xTaskGetTickCount();
            last_wake = last_discovery;
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CALCULATION_MS));
    }
    for (size_t i = 0U; i < MAX_NODES; ++i) {
        if (s_slots[i].imu != NULL && mpu6050_deinit_device(s_slots[i].imu) != 0) {
            LOGE("deinit failed token=0x%04X", s_slots[i].node->token);
        }
    }
    LOGI("test complete, printed=%u per discovered MPU6050", TEST_PRINTS);
    while (true) vTaskDelay(portMAX_DELAY);
}
