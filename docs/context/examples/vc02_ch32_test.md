# VC02 CH32 Test Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: vc02_ch32_test
purpose: Receive VC02 voice-recognition UART bytes through a CH32 UART gateway and dispatch demo action slots.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_vc02_gateway
protocol_contracts:
  - ch32_vc02_gateway
user_goal:
  - ESP32-WROOM receives recognized VC02 commands through CH32.
required_behavior:
  - ESP32 initializes the VC02 parser and callback table.
  - ESP32 sends periodic ping frames to the expected CH32 VC02 bridge.
  - ESP32 consumes VC02 UART bytes forwarded by CH32 on CAN.
  - ESP32 logs recognized commands and dispatches safe demo callbacks.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Feed forwarded UART bytes into the VC02 parser.
  - Match only documented VC02 byte sequences.
  - Dispatch registered callback slots and keep unknown bytes as no-op logs.
  - Print bridge status, ACK, parser status, and matched command logs.
ch32_responsibilities:
  - Own the downstream VC02 UART connection.
  - Forward VC02 UART RX bytes over CAN.
  - Return bridge ACK/status frames once the CH32 bridge firmware defines them.
serial_log:
  - VC02_EVENT name=<command> uart="<bytes>" meaning="<meaning>" slot=<slot> raw=[...]
  - ACTION_SLOT <slot>: <meaning> -> replace this callback with a real module function raw=[...]
  - VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
  - VC02_DRIVER_STATUS chunks=<n> bytes=<n> matched=<n> dispatched=<n> no_match=<n>
state_machine:
  - init_parser
  - ping_bridge
  - feed_uart_bytes
  - dispatch_known_command
  - ignore_unknown_bytes
failure_behavior:
  - Missing bridge ACK/status is logged but does not trigger downstream actions.
  - Unknown VC02 UART bytes are ignored.
  - Demo callbacks only print action slots; final projects must bind real selected modules explicitly.
must_not_include:
  - ESP32 direct VC02 UART access.
  - Automatic control of OLED, motor, servo, MPU6050, TOF, SYN6288, WiFi, or Bluetooth unless selected by a separate recipe.
compile_command: .\tools\build.ps1 example/vc02_ch32_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
