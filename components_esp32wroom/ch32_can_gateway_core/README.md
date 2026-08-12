# ch32_can_gateway_core

Transport-neutral ESP32 CAN infrastructure for CH32 gateways.

This component is the only owner of TWAI installation, the CAN receive task,
frame-routing queues, bus-off recovery, transmit accounting and the shared
multi-frame transaction mutex. I2C and UART gateway protocol components depend
on this core directly; they must not depend on each other.

Applications normally do not call this component directly. Initialize the
selected I2C or UART gateway layer, which initializes the core idempotently.
