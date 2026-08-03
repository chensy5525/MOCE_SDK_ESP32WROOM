#include "ch32_node_minimal.h"

static bool node_id_is_valid(uint8_t node_id)
{
    return node_id > 0U && node_id <= CH32_NODE_MINIMAL_NODE_ID_MAX;
}

bool ch32_node_minimal_parse_hello(uint32_t can_id,
                                   const uint8_t *data,
                                   uint8_t dlc,
                                   ch32_node_minimal_hello_t *hello)
{
    if (data == NULL || hello == NULL || dlc < 4U) {
        return false;
    }

    uint8_t node_id = data[1];
    if (!node_id_is_valid(node_id)) {
        return false;
    }
    if (can_id != CH32_NODE_MINIMAL_CAN_ID_HELLO(node_id)) {
        return false;
    }

    hello->device_type = data[0];
    hello->node_id = node_id;
    hello->fw_version = data[2];
    hello->capability_flags = data[3];
    return true;
}

bool ch32_node_minimal_parse_ack(uint32_t can_id,
                                 const uint8_t *data,
                                 uint8_t dlc,
                                 ch32_node_minimal_ack_t *ack)
{
    if (data == NULL || ack == NULL || dlc < 4U) {
        return false;
    }

    uint8_t node_id = data[2];
    if (!node_id_is_valid(node_id)) {
        return false;
    }
    if (can_id != CH32_NODE_MINIMAL_CAN_ID_ACK(node_id)) {
        return false;
    }

    ack->command_type = data[0];
    ack->result = data[1] != 0U;
    ack->node_id = node_id;
    ack->device_type = data[3];
    return true;
}

const char *ch32_node_minimal_device_type_text(uint8_t device_type)
{
    switch (device_type) {
    case CH32_NODE_MINIMAL_DEVICE_TYPE_I2C:
        return "I2C";
    case CH32_NODE_MINIMAL_DEVICE_TYPE_MOTOR:
        return "MOTOR";
    case CH32_NODE_MINIMAL_DEVICE_TYPE_SERVO:
        return "SERVO";
    default:
        return "UNKNOWN";
    }
}
