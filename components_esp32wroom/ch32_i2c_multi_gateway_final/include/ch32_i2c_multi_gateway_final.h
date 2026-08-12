#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_I2C_MULTI_MAX_NODES           6U
#define CH32_I2C_MULTI_MAX_ADDRS_PER_NODE  8U
#define CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C 0x01U
#define CH32_CAN_GATEWAY_BITRATE_HZ        500000U
#define CH32_CAN_GATEWAY_MAX_FRAME_DATA    8U

typedef enum {
    CH32_I2C_MULTI_RESULT_OK = 0,
    CH32_I2C_MULTI_RESULT_TIMEOUT,
    CH32_I2C_MULTI_RESULT_COMM_FAIL,
    CH32_I2C_MULTI_RESULT_NO_DATA,
    CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND,
    CH32_I2C_MULTI_RESULT_INVALID_ARG,
    CH32_I2C_MULTI_RESULT_BUSY,
} ch32_i2c_multi_result_t;

typedef enum {
    CH32_I2C_MULTI_ROLE_MASTER = 0,
    CH32_I2C_MULTI_ROLE_SLAVE,
} ch32_i2c_multi_role_t;

typedef enum {
    CH32_I2C_MULTI_STATUS_ISR_RX = 0,
    CH32_I2C_MULTI_STATUS_ROUTE_DATA,
    CH32_I2C_MULTI_STATUS_ROUTE_I2C,
    CH32_I2C_MULTI_STATUS_ROUTE_UART,
    CH32_I2C_MULTI_STATUS_ROUTE_OBSERVER,
    CH32_I2C_MULTI_STATUS_APP_RX,
    CH32_I2C_MULTI_STATUS_TX,
} ch32_i2c_multi_status_field_t;

typedef struct {
    uint32_t id;
    uint8_t  data[CH32_CAN_GATEWAY_MAX_FRAME_DATA];
    uint8_t  dlc;
    bool     extd;
} ch32_i2c_multi_can_frame_t;

typedef struct {
    uint8_t  node_id;
    uint16_t token;
    uint8_t  i2c_addrs[CH32_I2C_MULTI_MAX_ADDRS_PER_NODE];
    uint8_t  i2c_addr_count;
    bool     ready;
    uint8_t  device_type;
    uint8_t  fw_version;
    uint8_t  capability_flags;
} ch32_i2c_multi_node_t;

typedef struct {
    uint32_t discovery_timeout_ms;
    uint32_t command_timeout_ms;
    uint8_t  _reserved[8];
} ch32_i2c_multi_config_t;

/* ── Init ────────────────────────── */
void ch32_i2c_multi_default_config(ch32_i2c_multi_config_t *cfg);
int  ch32_i2c_multi_init(const ch32_i2c_multi_config_t *cfg);

/* ── Discovery ────────────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_discover_and_assign(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count, uint32_t timeout_ms);
ch32_i2c_multi_result_t ch32_i2c_multi_discover_incremental(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count, uint32_t timeout_ms);
ch32_i2c_multi_result_t ch32_i2c_multi_scan(ch32_i2c_multi_node_t *node);
ch32_i2c_multi_result_t ch32_i2c_multi_probe(
    const ch32_i2c_multi_node_t *node, uint8_t addr, bool *found);

/* ── I2C operations ──────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_write_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_reg_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_raw(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_raw_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_from(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_once(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_read_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg, const uint8_t *wdata, uint8_t wlen,
    uint8_t *rdata, uint8_t rlen);

/* ── Speed ───────────────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_100k(ch32_i2c_multi_node_t *node);
ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_400k(ch32_i2c_multi_node_t *node);

/* ── CAN frame I/O ────────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_poll_can_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);

/* Shared CAN gateway functions (used by both I2C and UART components) */
ch32_i2c_multi_result_t ch32_can_gateway_send_frame(
    uint32_t id, const uint8_t *data, uint8_t dlc);
ch32_i2c_multi_result_t ch32_can_gateway_poll_control_frame(
    uint8_t device_type, ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);
ch32_i2c_multi_result_t ch32_can_gateway_poll_observed_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);
void ch32_can_gateway_set_observer_filter(uint32_t filter_id);

/* ── Status ──────────────────────── */
uint32_t ch32_i2c_multi_get_status(ch32_i2c_multi_status_field_t field);

/* ── Strings ─────────────────────── */
const char *ch32_i2c_multi_result_text(ch32_i2c_multi_result_t result);
const char *ch32_i2c_multi_role_text(ch32_i2c_multi_role_t role);

#ifdef __cplusplus
}
#endif
