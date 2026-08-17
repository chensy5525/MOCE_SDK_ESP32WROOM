#ifndef CH32_VL53L4CD_GATEWAY_H__
#define CH32_VL53L4CD_GATEWAY_H__
#include <stdbool.h>
#include <stdint.h>
#include "ch32_i2c_multi_gateway_final.h"
#include "module_errors.h"
#define ERR_CH32_VL53L4CD_OUT_OF_RANGE -20
#define ERR_CH32_VL53L4CD_ID_MISMATCH -21
#define ERR_CH32_VL53L4CD_DATA_NOT_READY -22
#define ERR_CH32_VL53L4CD_COMM -23
#define ERR_CH32_VL53L4CD_INVALID_DATA -24
#define CH32_VL53L4CD_I2C_ADDR_DEFAULT 0x29U
#define CH32_VL53L4CD_GATEWAY_TIMEOUT_MS 350U
#define CH32_VL53L4CD_DATA_READY_TIMEOUT_MS 400U
#define CH32_VL53L4CD_INIT_TIMEOUT_MS 3500U
#define CH32_VL53L4CD_READ_MAX_ATTEMPTS 2U
#define CH32_VL53L4CD_RANGE_MAX_MM 1300U
#define CH32_VL53L4CD_MODEL_ID 0xEBAAU
#define CH32_VL53L4CD_MAX_INSTANCES 6U
typedef struct {
    uint8_t i2c_addr;
    ch32_i2c_multi_node_t *ch32_node;
    uint32_t data_ready_timeout_ms;
} ch32_vl53l4cd_cfg_t;
typedef struct {
    uint16_t distance_mm;
    uint8_t range_status;
    bool valid;
    bool out_of_range;
} ch32_vl53l4cd_result_t;

typedef struct ch32_vl53l4cd_ctx *ch32_vl53l4cd_handle_t;

int ch32_vl53l4cd_init(ch32_vl53l4cd_handle_t *handle,
                       const ch32_vl53l4cd_cfg_t *cfg);
int ch32_vl53l4cd_deinit(ch32_vl53l4cd_handle_t handle);
int ch32_vl53l4cd_read(ch32_vl53l4cd_handle_t handle,
                       ch32_vl53l4cd_result_t *result);

#endif
