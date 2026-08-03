# ch32_node_minimal

Minimal ESP32-side helper for CH32 CAN node discovery.

This component does not control a concrete peripheral. It only parses the
common CH32 node protocol:

- `0x700 + NODE_ID`: HELLO / HEARTBEAT
- `0x500 + NODE_ID`: ACK

Use it before writing a full peripheral example to confirm that ESP32-WROOM
can receive frames from one or more CH32 nodes and can identify their
`NODE_ID`, `DEVICE_TYPE`, firmware version, and capability flags.
