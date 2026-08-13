# Board Classification, Roles & Wiring

## SECTION 1: Category Overview

- **main-controller**: Runs main logic, manages all sub-modules
- **auxiliary**: Signal conversion / routing / interface expansion only. No business logic.
- **sensor**: Collects data from environment, one-way input. Subtypes: data-type (returns numeric values) and event-type (returns state changes or strings)
- **actuator**: Acts on environment, one-way output
- **driver**: High-power device control, typically with independent power supply
- **communication**: Bidirectional data channel, core function is send/receive
- **hmi**: Human-machine interface. Subtypes: input-type (buttons, knobs) and output-type (OLED, LCD)

## SECTION 2: Role & Typical Devices

- main-controller: ESP32-WROOM. Runs main logic, device table owner. All modules ultimately communicate with it.
- auxiliary: CH32V203 (CAN-to-I2C/UART/SPI bridge, transparent forwarding, no business logic), I2C MUX, level shifters
- sensor-data: BMP280 (pressure), TMP117 (temperature), VL53L0X (laser ranging)
- sensor-event: voice recognition module, PIR motion sensor, rotary encoder
- actuator: relay module, solenoid valve
- driver: A4950 (motor driver), servo driver board
- communication: HM-10 BLE, WiFi module, LoRa, RS485
- hmi-input: buttons, knobs, touch
- hmi-output: OLED, LCD

## SECTION 3: Typical Wiring

- ESP32 main controller: All modules connect directly or via CH32 bridge, ultimately converging in ESP32's device table.
- CH32 bridge: ESP32 communicates with each CH32 via CAN. CH32 converts CAN frames to the downstream module's bus protocol. One CH32 supports one downstream module. Transparent forwarding only.
- I2C modules: Connect to ESP32 local I2C or CH32 downstream I2C. Distinguished by device address.
- UART modules: Connect to ESP32 local UART or CH32 downstream UART. No address; identity determined by protocol frame content.
- Driver boards: Typically PWM + GPIO (direct to ESP32). Independent power supply, common ground with main controller.
