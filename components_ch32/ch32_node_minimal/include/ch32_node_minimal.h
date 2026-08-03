#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_NODE_MINIMAL_NODE_ID_MAX       10U
#define CH32_NODE_MINIMAL_FRAME_DLC         8U
#define CH32_NODE_MINIMAL_DEVICE_TYPE_I2C   0x01U
#define CH32_NODE_MINIMAL_DEVICE_TYPE_MOTOR 0x02U
#define CH32_NODE_MINIMAL_DEVICE_TYPE_SERVO 0x03U

#define CH32_NODE_MINIMAL_CAN_ID_STATUS(node_id) (0x100U + (uint32_t)(node_id))
#define CH32_NODE_MINIMAL_CAN_ID_I2C_CMD(node_id) (0x200U + (uint32_t)(node_id))
#define CH32_NODE_MINIMAL_CAN_ID_MOTOR_CMD(node_id) (0x300U + (uint32_t)(node_id))
#define CH32_NODE_MINIMAL_CAN_ID_SERVO_CMD(node_id) (0x400U + (uint32_t)(node_id))
#define CH32_NODE_MINIMAL_CAN_ID_ACK(node_id) (0x500U + (uint32_t)(node_id))
#define CH32_NODE_MINIMAL_CAN_ID_HELLO(node_id) (0x700U + (uint32_t)(node_id))

typedef struct {
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
} ch32_node_minimal_hello_t;

typedef struct {
    uint8_t command_type;
    bool result;
    uint8_t node_id;
    uint8_t device_type;
} ch32_node_minimal_ack_t;

bool ch32_node_minimal_parse_hello(uint32_t can_id,
                                   const uint8_t *data,
                                   uint8_t dlc,
                                   ch32_node_minimal_hello_t *hello);

bool ch32_node_minimal_parse_ack(uint32_t can_id,
                                 const uint8_t *data,
                                 uint8_t dlc,
                                 ch32_node_minimal_ack_t *ack);

const char *ch32_node_minimal_device_type_text(uint8_t device_type);

#ifdef __cplusplus
}
#endif
