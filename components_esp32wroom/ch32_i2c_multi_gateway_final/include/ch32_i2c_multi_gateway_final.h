#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ch32_can_gateway_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_I2C_MULTI_MAX_NODES           6U
#define CH32_I2C_MULTI_MAX_ADDRS_PER_NODE  8U
#define CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C 0x01U
#define CH32_I2C_MULTI_PROTOCOL_V1          1U
#define CH32_I2C_MULTI_PROTOCOL_V2          2U
#define CH32_CAN_GATEWAY_BITRATE_HZ        CH32_CAN_GATEWAY_CORE_BITRATE_HZ
#define CH32_CAN_GATEWAY_MAX_FRAME_DATA    CH32_CAN_GATEWAY_CORE_FRAME_DATA_MAX

typedef enum {
    CH32_I2C_MULTI_RESULT_OK = 0,
    CH32_I2C_MULTI_RESULT_TIMEOUT,
    CH32_I2C_MULTI_RESULT_COMM_FAIL,
    CH32_I2C_MULTI_RESULT_NO_DATA,
    CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND,
    CH32_I2C_MULTI_RESULT_INVALID_ARG,
    CH32_I2C_MULTI_RESULT_BUSY,
    CH32_I2C_MULTI_RESULT_OVERFLOW,
} ch32_i2c_multi_result_t;

typedef enum {
    CH32_I2C_MULTI_ROLE_MASTER = 0,
    CH32_I2C_MULTI_ROLE_SLAVE,
} ch32_i2c_multi_role_t;

typedef enum {
    CH32_I2C_MULTI_STATUS_ISR_RX = 0,
    CH32_I2C_MULTI_STATUS_ROUTE_DATA,
    CH32_I2C_MULTI_STATUS_ROUTE_I2C,
    CH32_I2C_MULTI_STATUS_ROUTE_SPI,
    CH32_I2C_MULTI_STATUS_ROUTE_UART,
    CH32_I2C_MULTI_STATUS_ROUTE_OBSERVER,
    CH32_I2C_MULTI_STATUS_APP_RX,
    CH32_I2C_MULTI_STATUS_TX,
    CH32_I2C_MULTI_STATUS_QUEUE_DROPS,
} ch32_i2c_multi_status_field_t;

typedef ch32_can_gateway_frame_t ch32_i2c_multi_can_frame_t;

typedef struct {
    uint8_t  node_id;
    uint16_t token;
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    uint8_t  i2c_addrs[CH32_I2C_MULTI_MAX_ADDRS_PER_NODE];
    uint8_t  i2c_addr_count;
#endif
    bool     ready;
    uint8_t  device_type;
    uint8_t  fw_version;
    uint8_t  capability_flags;
    uint8_t  protocol_version;
    uint8_t  active_speed_code;
    bool     speed_valid;
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
    bool     scan_bus_diag_valid;
    bool     scan_scl_before_high;
    bool     scan_sda_before_high;
    bool     scan_recovery_ok;
    bool     scan_scl_after_high;
    bool     scan_sda_after_high;
    bool     scan_gpio_ack_normal;
    bool     scan_gpio_ack_swapped;
#endif
} ch32_i2c_multi_node_t;

typedef struct {
    uint8_t i2c_addrs[CH32_I2C_MULTI_MAX_ADDRS_PER_NODE];
    uint8_t i2c_addr_count;
    bool bus_diag_valid;
    bool scl_before_high;
    bool sda_before_high;
    bool recovery_ok;
    bool scl_after_high;
    bool sda_after_high;
    bool gpio_ack_normal;
    bool gpio_ack_swapped;
} ch32_i2c_multi_scan_result_t;

typedef struct {
    uint32_t discovery_timeout_ms;
    uint32_t command_timeout_ms;
    uint16_t discovery_query_interval_ms;
    uint16_t discovery_quiet_ms;
    uint8_t  discovery_min_rounds;
    uint8_t  _reserved[3];
} ch32_i2c_multi_config_t;

/* ── Init ────────────────────────── */
void ch32_i2c_multi_default_config(ch32_i2c_multi_config_t *cfg);
int  ch32_i2c_multi_init(const ch32_i2c_multi_config_t *cfg);

/* ── Discovery ────────────────────── */
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_i2c_multi_result_t ch32_i2c_multi_discover_and_assign(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count, uint32_t timeout_ms);
#endif
ch32_i2c_multi_result_t ch32_i2c_multi_discover_incremental(
    ch32_i2c_multi_node_t *nodes, uint8_t max_nodes, size_t *count, uint32_t timeout_ms);
ch32_i2c_multi_result_t ch32_i2c_multi_scan(
    const ch32_i2c_multi_node_t *node, ch32_i2c_multi_scan_result_t *scan_result);
ch32_i2c_multi_result_t ch32_i2c_multi_probe(
    const ch32_i2c_multi_node_t *node, uint8_t addr, bool *found);
ch32_i2c_multi_result_t ch32_i2c_multi_probe_timeout(
    const ch32_i2c_multi_node_t *node, uint8_t addr, bool *found,
    uint32_t timeout_ms);

/* ── I2C operations ──────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_write_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_reg_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_reg_to_timeout(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    const uint8_t *data, uint8_t len, uint32_t timeout_ms);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_to_timeout(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *data, uint8_t len, uint32_t timeout_ms);
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_i2c_multi_result_t ch32_i2c_multi_write_raw(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_raw_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len);
#endif
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_from(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_from_timeout(
    const ch32_i2c_multi_node_t *node, uint8_t addr, uint8_t reg,
    uint8_t *data, uint8_t len, uint32_t timeout_ms);
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_i2c_multi_result_t ch32_i2c_multi_read_regs_once(
    const ch32_i2c_multi_node_t *node, uint8_t reg, uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_multi_once(
    const ch32_i2c_multi_node_t *node, const uint8_t *data, uint8_t len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_read_reg(
    const ch32_i2c_multi_node_t *node, uint8_t reg, const uint8_t *wdata, uint8_t wlen,
    uint8_t *rdata, uint8_t rlen);
#endif
ch32_i2c_multi_result_t ch32_i2c_multi_write_read_to(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *write_data, uint8_t write_len,
    uint8_t *read_data, uint8_t read_len);
ch32_i2c_multi_result_t ch32_i2c_multi_write_read_to_timeout(
    const ch32_i2c_multi_node_t *node, uint8_t addr,
    const uint8_t *write_data, uint8_t write_len,
    uint8_t *read_data, uint8_t read_len, uint32_t timeout_ms);

/* ── Speed ───────────────────────── */
ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_100k(ch32_i2c_multi_node_t *node);
ch32_i2c_multi_result_t ch32_i2c_multi_set_speed_400k(ch32_i2c_multi_node_t *node);

#if CH32_CAN_GATEWAY_LEGACY_COMPAT
/* Legacy aliases and raw CAN exposure. New module code must use the canonical
 * explicit-address operations above. */
ch32_i2c_multi_result_t ch32_i2c_multi_poll_can_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);

/* Shared CAN gateway functions (used by both I2C and UART components) */
ch32_i2c_multi_result_t ch32_can_gateway_send_frame(
    uint32_t id, const uint8_t *data, uint8_t dlc);
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_i2c_multi_result_t ch32_can_gateway_poll_control_frame(
    uint8_t device_type, ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);
#endif
ch32_i2c_multi_result_t ch32_can_gateway_poll_observed_frame(
    ch32_i2c_multi_can_frame_t *frame, uint32_t timeout_ms);
void ch32_can_gateway_set_observer_filter(uint32_t filter_id);
#endif

/* ── Status ──────────────────────── */
uint32_t ch32_i2c_multi_get_status(ch32_i2c_multi_status_field_t field);

/* ── Strings ─────────────────────── */
const char *ch32_i2c_multi_result_text(ch32_i2c_multi_result_t result);
const char *ch32_i2c_multi_role_text(ch32_i2c_multi_role_t role);

#ifdef __cplusplus
}
#endif
