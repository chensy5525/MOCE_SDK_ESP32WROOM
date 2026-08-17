# ch32_can_gateway_core

Transport-neutral ESP32 CAN infrastructure for CH32 gateways.

This component is the only owner of TWAI installation, the CAN receive task,
frame routing, bus-off recovery and transmit accounting. I2C, SPI and UART
gateway protocol components depend on this core directly; they must not depend
on each other.

Runtime responses are routed into per-node sessions. Operations to the same
physical CH32 node remain serialized, while different I2C, SPI and UART nodes
can progress independently. UART payload RX also has its own queue so business
data cannot consume transfer acknowledgements. The old global transaction lock
is retained only for source compatibility and must not be used by new gateway
code.

Applications normally do not call this component directly. Initialize the
selected I2C, SPI or UART gateway layer, which initializes the core
idempotently.
