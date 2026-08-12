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
#define CH32_CAN_GATEWAY_DEVICE_TYPE_UART    0x04U

typedef enum {
    CH32_CAN_GATEWAY_OK = 0,
    CH32_CAN_GATEWAY_TIMEOUT,
    CH32_CAN_GATEWAY_COMM_FAIL,
    CH32_CAN_GATEWAY_NO_DATA,
    CH32_CAN_GATEWAY_INVALID_ARG,
    CH32_CAN_GATEWAY_BUSY,
} ch32_can_gateway_result_t;

typedef enum {
    CH32_CAN_GATEWAY_STATUS_ISR_RX = 0,
    CH32_CAN_GATEWAY_STATUS_ROUTE_DATA,
    CH32_CAN_GATEWAY_STATUS_ROUTE_I2C,
    CH32_CAN_GATEWAY_STATUS_ROUTE_UART,
    CH32_CAN_GATEWAY_STATUS_ROUTE_OBSERVER,
    CH32_CAN_GATEWAY_STATUS_APP_RX,
    CH32_CAN_GATEWAY_STATUS_TX,
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
ch32_can_gateway_result_t ch32_can_gateway_core_poll_control(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms);
ch32_can_gateway_result_t ch32_can_gateway_core_poll_observer(
    uint8_t device_type, ch32_can_gateway_frame_t *frame,
    uint32_t timeout_ms);
ch32_can_gateway_result_t ch32_can_gateway_core_poll_data(
    ch32_can_gateway_frame_t *frame, uint32_t timeout_ms);

ch32_can_gateway_result_t ch32_can_gateway_core_lock(uint32_t timeout_ms);
void ch32_can_gateway_core_unlock(void);
uint32_t ch32_can_gateway_core_get_status(
    ch32_can_gateway_status_field_t field);

#ifdef __cplusplus
}
#endif

#endif
