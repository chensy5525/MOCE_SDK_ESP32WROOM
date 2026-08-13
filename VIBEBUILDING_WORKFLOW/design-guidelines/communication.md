# Communication Module Driver Guidelines

## SECTION 1: Communication vs. Sensor -- Key Difference

- Sensor read() returns environmental data collected by the module itself
- Communication module read() returns "data sent by the remote peer". The module is only a channel; it does not generate data.
- Communication module write() sends data to the remote peer. Sensors generally do not implement write().
- Communication modules may actively push data at any time (from remote peer). They are not poll-only. Therefore a receive buffer or callback mechanism is required.

## SECTION 2: General Flow

- init: configure physical interface -> configure module operating params (baud/mode/broadcast interval etc.) -> check module presence (AT command or read ID register) -> initialize receive buffer -> mark initialized
- deinit: close connection (if any) -> flush buffer -> release bus resources
- read: retrieve data from receive buffer. If buffer empty: return 0 (no data) or `ERR_TIMEOUT` (waited specified time with no data). Never block indefinitely.
- write: assemble protocol frame -> send -> optionally wait for peer ACK
- reset: restore factory defaults -> reconfigure -> flush buffer -> reconnect

## SECTION 3: Connection Management & State Machine

- Communication module maintains at minimum 4 states: NOT_INIT -> READY (configured) -> CONNECTED -> FAULT
- Auto-detect disconnection (timeout with no data / AT query connection status / hardware indicator pin). Log `[WRN]` on detection.
- Reconnection strategy: retry interval and max retries documented in card.md. Log `[ERR]` and enter FAULT state when retries exhausted.
- Connection state exposed to upper layer via read() or dedicated status query. Upper layer decides whether to reconnect.

## SECTION 4: Receive Buffer

- Communication module MUST have a receive ring buffer. ISR or polling receives data into buffer first; upper layer read() retrieves from buffer.
- Buffer size documented in card.md, based on max frame length and data throughput.
- Buffer full behavior: overwrite oldest data (head chases tail) or discard and log `[WRN]`.
- Support frame boundary detection: if the module's protocol has clear frame delimiters (e.g. `\r\n` or fixed header), read() should return complete frames, not partial data.

## SECTION 5: Interface-Specific Notes (MCU <-> Module)

- **UART**: Most common for communication modules (HM-10 BLE, WiFi, LoRa, RS485). Both sides must match baud rate. Note: module factory default baud may differ from project target. During init, first query at default baud via AT, confirm, then switch.
- **SPI**: A few high-speed communication modules (e.g. SPI WiFi). Distinguished by CS. Mind CPOL/CPHA. SPI communication modules often have an interrupt pin to notify main controller of new data.
- **I2C**: Rare. Distinguished by device address. Mind I2C speed limitation -- I2C can be a bottleneck for high-throughput data transfer.

## SECTION 6: Module-Type-Specific Notes

- **BLE transparent modules (e.g. HM-10)**: MCU side uses UART; remote peer is BLE. Must configure broadcast name, broadcast interval, connection params. AT commands enter config mode; exit returns to transparent mode. In transparent mode, all send/receive is peer data; no AT parsing.
- **WiFi modules**: Must configure SSID, password, IP mode. Connection establishment takes longer; init must wait for DHCP or static IP ready. TCP/UDP connection management more complex than BLE; handle socket state.
- **LoRa modules**: Low speed, long range. Mind air rate and frequency band config. Send and receive cannot happen simultaneously; half-duplex management required. After send, wait for send-complete interrupt or timeout.
- **RS485**: Half-duplex differential bus. Must control TX enable pin. Pull TX_EN high before send; pull low after send complete to return to receive mode. Mind bus termination and bias resistors. Multiple devices share the bus; need protocol-layer address for identity.

## SECTION 7: General Notes

- Communication module config params (baud, device name, broadcast interval etc.) as #define grouped at file top.
- Wireless modules: mind antenna placement and power supply stability. Insufficient power causes unstable connections.
- If module factory defaults differ from project target params, document in card.md as "default -> target".
- CAN modules are NOT covered here -- in this project, CAN is the system bus between ESP32 and CH32, not a peripheral communication module.

## SECTION 8: Direct vs. Bridged Versions

Direct and bridge versions are independent software packages:
- `<abbr>_direct.c` — module directly connected to ESP32 UART/SPI
- `<abbr>_bridge.c` — module connected downstream of a CH32-UART bridge

**Key differences in bridge version:**
- UART transport uses the real `ch32_uart_dynamic_gateway_final` public API.
  The gateway layer owns START/DATA/ACK, CRC and RX routing; the module driver
  does not construct CAN frames.
- Handle holds a stable CH32 node reference and `bridge_timeout_ms` instead of `uart_port`; it does not cache a copied runtime node_id
- Ring buffer still local (ESP32 side) but populated from CAN RX callback instead of local UART ISR
- `_init` configures CH32-UART bridge baud rate via CAN command, then configures the downstream module
- AT command sequences call `ch32_uart_dynamic_send()` (or another current
  public API verified in the target repository); do not invent a generic
  `can_send_uart_data()` helper.
- If target baud differs from default: AT switch-baud command must be coordinated with CH32 baud change
- RS485 TX_EN timing is handled by CH32 firmware (not ESP32 side)
- `_init` receives the stable node reference from device discovery and validates F2-confirmed `ready`, token, and the gateway-type node_id range

**Templates:**
- `_templates/communication_direct.c`
- `_templates/communication_bridge.c`
