#ifndef CH32_CAN_GATEWAY_CORE_H
#define CH32_CAN_GATEWAY_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_CAN_GATEWAY_CORE_BITRATE_HZ     500000U
#define CH32_CAN_GATEWAY_CORE_FRAME_DATA_MAX 8U
#define CH32_CAN_GATEWAY_DEVICE_TYPE_I2C     0x01U
#define CH32_CAN_GATEWAY_DEVICE_TYPE_SPI     0x03U
#define CH32_CAN_GATEWAY_DEVICE_TYPE_UART    0x04U

/* Disabled by default: old global response queues and the core-wide lock.
 * Define to 1 only while migrating legacy application code. */
#ifndef CH32_CAN_GATEWAY_LEGACY_COMPAT
#define CH32_CAN_GATEWAY_LEGACY_COMPAT 0
#endif

typedef enum {
    CH32_CAN_GATEWAY_OK = 0,
    CH32_CAN_GATEWAY_TIMEOUT,
    CH32_CAN_GATEWAY_COMM_FAIL,
    CH32_CAN_GATEWAY_NO_DATA,
    CH32_CAN_GATEWAY_INVALID_ARG,
    CH32_CAN_GATEWAY_BUSY,
    CH32_CAN_GATEWAY_OVERFLOW,
} ch32_can_gateway_result_t;

typedef enum {
    CH32_CAN_GATEWAY_STATUS_ISR_RX = 0,
    CH32_CAN_GATEWAY_STATUS_ROUTE_DATA,
    CH32_CAN_GATEWAY_STATUS_ROUTE_I2C,
    CH32_CAN_GATEWAY_STATUS_ROUTE_SPI,
    CH32_CAN_GATEWAY_STATUS_ROUTE_UART,
    CH32_CAN_GATEWAY_STATUS_ROUTE_OBSERVER,
    CH32_CAN_GATEWAY_STATUS_APP_RX,
    CH32_CAN_GATEWAY_STATUS_TX,
    CH32_CAN_GATEWAY_STATUS_QUEUE_DROPS,
} ch32_can_gateway_status_field_t;

typedef struct {
    uint32_t id;
    uint8_t data[CH32_CAN_GATEWAY_CORE_FRAME_DATA_MAX];
    uint8_t dlc;
    bool extd;
} ch32_can_gateway_frame_t;

int ch32_can_gateway_core_init(void);
bool ch32_can_gateway_core_is_initialized(void);

ch32_can_gateway_result_t ch32_can_gateway_core_send(
    uint32_t id, const uint8_t *data, uint8_t dlc);
ch32_can_gateway_result_t ch32_can_gateway_core_send_timeout(
    uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t timeout_ms);
#if CH32_CAN_GATEWAY_LEGACY_COMPAT
ch32_can_gateway_result_t ch32_can_gateway_core_poll_control(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms);
#endif
ch32_can_gateway_result_t ch32_can_gateway_core_poll_observer(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms);
ch32_can_gateway_result_t ch32_can_gateway_core_poll_data(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms);

/* A session serializes only one physical CH32 route. Acquire it before the
 * first command frame and release it after completion or failure. I2C, SPI and
 * UART nodes therefore no longer share one remote-response mutex. */
ch32_can_gateway_result_t ch32_can_gateway_core_session_acquire(
    uint8_t device_type, uint8_t node_id, uint32_t timeout_ms);
ch32_can_gateway_result_t ch32_can_gateway_core_session_poll(
    uint8_t device_type, uint8_t node_id, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms);
void ch32_can_gateway_core_session_drain(
    uint8_t device_type, uint8_t node_id);
void ch32_can_gateway_core_session_invalidate(
    uint8_t device_type, uint8_t node_id);
void ch32_can_gateway_core_session_release(
    uint8_t device_type, uint8_t node_id);

/* UART business RX is deliberately separate from UART transfer ACKs. */
ch32_can_gateway_result_t ch32_can_gateway_core_poll_uart_rx(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms);

#if CH32_CAN_GATEWAY_LEGACY_COMPAT
/* Deprecated compatibility lock. New protocol code uses node sessions above. */
ch32_can_gateway_result_t ch32_can_gateway_core_lock(uint32_t timeout_ms);
void ch32_can_gateway_core_unlock(void);
#endif
uint32_t ch32_can_gateway_core_get_status(
    ch32_can_gateway_status_field_t field);

#ifdef __cplusplus
}
#endif

#endif
