#ifndef CH32_VL53L4CD_GATEWAY_H__
#define CH32_VL53L4CD_GATEWAY_H__
#include <stdbool.h>
#include <stdint.h>
#include "ch32_i2c_multi_gateway_final.h"
#ifndef ERR_TIMEOUT
#define ERR_TIMEOUT -1
#define ERR_BUSY -2
#define ERR_NOT_INIT -3
#define ERR_INVALID_PARAM -4
#define ERR_NOT_SUPPORTED -5
#define ERR_OVERFLOW -6
#define ERR_NO_DEVICE -7
#define ERR_HW_FAULT -8
#endif
#define ERR_VL53L4CD_OUT_OF_RANGE -20
#define ERR_VL53L4CD_ID_MISMATCH -21
#define ERR_VL53L4CD_DATA_NOT_READY -22
#define ERR_VL53L4CD_COMM -23
#define ERR_VL53L4CD_INVALID_DATA -24
#define VL53L4CD_I2C_ADDR_DEFAULT 0x29U
#define VL53L4CD_BRIDGE_TIMEOUT_MS 2000U
#define VL53L4CD_DATA_READY_TIMEOUT_MS 400U
#define VL53L4CD_MAX_RETRIES 3U
#define VL53L4CD_RANGE_MAX_MM 1300U
#define VL53L4CD_MODEL_ID 0xEBAAU
#define VL53L4CD_MAX_BRIDGE_INSTANCES 6U
typedef struct { uint8_t i2c_addr; ch32_i2c_multi_node_t *ch32_node; uint32_t bridge_timeout_ms; uint32_t data_ready_timeout_ms; } vl53l4cd_cfg_t;
typedef struct { uint16_t distance_mm; uint8_t range_status; bool valid; bool out_of_range; } vl53l4cd_result_t;
typedef struct vl53l4cd_ctx *vl53l4cd_handle_t;
int vl53l4cd_init(vl53l4cd_handle_t *handle,const vl53l4cd_cfg_t *cfg);
int vl53l4cd_deinit(vl53l4cd_handle_t handle);
int vl53l4cd_read(vl53l4cd_handle_t handle,vl53l4cd_result_t *result);
#endif
