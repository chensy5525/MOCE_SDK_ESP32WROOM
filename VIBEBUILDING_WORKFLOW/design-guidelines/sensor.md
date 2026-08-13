# Sensor Driver Guidelines

## SECTION 1: General Flow

- init: configure comm interface -> read WHO_AM_I (if available) to verify device presence -> wait power-up stabilization -> write config registers (if needed) -> mark initialized
- deinit: release bus resources. No complex operations.
- read: read data registers -> convert to physical quantity -> range check -> return result
- Sensors generally do NOT implement write (they are not actuators) and do NOT need reset (unless the hardware has a dedicated reset pin)

## SECTION 2: Data-Type Sensors

- read returns physical quantity (temperature in C, pressure in hPa, distance in mm). Never return raw register values.
- Conversion formula documented in comments, with datasheet page reference.
- Out-of-range values return module-specific error code (e.g. `ERR_VL53L0X_OUT_OF_RANGE`). Do not return clamped values.
- Support continuous reading. Each read() call triggers one sample.

## SECTION 3: Event-Type Sensors

- read returns event state or recognition result string
- Core difference from data-type: data-type returns numeric values; event-type returns state transitions or text
- Examples: voice module returns recognized string; PIR returns motion-detected/clear state; rotary encoder returns accumulated count
- Events may not produce new data on every read(). Agree on "what to return when no event": recommended to return 0 or empty string, and log the event itself

## SECTION 4: Interface-Specific Notes

- **I2C**: Distinguished by device address. No address conflicts on same bus. Default speed 100kHz; speeds above 400kHz must be noted in card.md. SDA/SCL need pull-up resistors. Multi-byte reads: note register auto-increment behavior.
- **UART**: No device address. Identity via protocol frame content. Both sides must agree on baud rate, data bits, stop bits, parity. Module may actively push data; design receive buffer or frame parser to prevent data sticking or frame loss.
- **SPI**: Distinguished by CS pin. Multiple devices on same SPI bus, each with its own CS. Note CPOL/CPHA combinations -- must match datasheet exactly. SPI mode (0/1/2/3) documented in card.md. Some sensors are half-duplex or 3-wire SPI; must be explicitly noted.
- **GPIO/Pulse**: Rotary encoders and similar pulse-output sensors typically use GPIO interrupt + timer capture. Keep interrupt frequency manageable to avoid missed pulses; use hardware counter peripherals if needed. ISR only counts; no complex processing.
- **Analog**: Analog voltage output sensors (e.g. analog temp probe, photoresistor) require ADC read and conversion. Note reference voltage, voltage divider circuit, sample rate. ADC conversion formula and reference voltage documented in comments.

## SECTION 5: Interface-Independent Notes

- Must wait stabilization time after power-up before reading. Time value in card.md.
- If pins can be changed via jumpers/resistors, document in card.md.
- Do not retry indefinitely on read failure. Set retry limit (default 3). Return `ERR_TIMEOUT` when exceeded.

## SECTION 6: Direct vs. Bridged Versions

Direct and bridge versions are independent software packages, each with its
own implementation, public header, CMake definition and minimum example:
- `<abbr>_direct.c` — for modules directly connected to ESP32 (I2C/UART/SPI/GPIO/ADC)
- `<abbr>_bridge.c` — for modules connected downstream of a CH32 CAN bridge

Operation semantics should remain consistent where practical, but cfg types and
transport-specific public types do not have to be identical.  Direct code uses
the current BSP; bridge code uses the matching verified gateway protocol layer.

**Key differences in bridge version:**
- I2C operations use `ch32_i2c_multi_gateway_final`; UART operations use
  `ch32_uart_dynamic_gateway_final`.  A module driver never constructs CAN
  frames or calls the CAN core directly.
- Handle struct holds a stable CH32 node reference and `bridge_timeout_ms` instead of `i2c_port`/`uart_port`; do not cache a copied runtime node_id
- `_init` receives the stable node reference from the device discovery layer (device-discovery.md). Driver does NOT discover and reads the current node_id from that record
- Bridge timeout is typically 1.5–2x direct timeout (exact value in card.md)
- `_init` must validate the node reference, F2-confirmed `ready`, token, and gateway-type node_id range before proceeding
- Remote enable/reset is generated only when authoritative CH32 firmware and a
  matching ESP32 protocol API actually implement it.  Do not invent a generic
  `can_send_gpio_write()` helper.

**Which template to use:**
- Direct version: `_templates/sensor_direct.c`
- Bridge version: `_templates/sensor_bridge.c`

AI generates one or both depending on user instruction. The two `.c` files are independent and do not include each other.
