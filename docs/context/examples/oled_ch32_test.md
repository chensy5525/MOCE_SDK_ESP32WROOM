# OLED CH32 Gateway Test Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: oled_ch32_test
purpose: Display a fixed test message on an SSD1315 OLED through a CH32 I2C gateway.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_oled_gateway
protocol_contracts:
  - ch32_oled_gateway
user_goal:
  - ESP32-WROOM displays text on an OLED connected to CH32.
required_behavior:
  - ESP32 waits for the CH32 I2C gateway HELLO frame.
  - ESP32 probes OLED address 0x3C through CH32.
  - ESP32 sends SSD1315 initialization and display bytes as documented CH32 raw I2C write commands.
  - OLED shows the expected test text.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Detect the CH32 I2C gateway node from HELLO frames.
  - Encode documented OLED probe and raw-write CAN frames.
  - Check ACK/status for every OLED operation.
  - Log command failure without assuming the screen changed.
ch32_responsibilities:
  - Own the downstream I2C bus.
  - Write raw SSD1315 control/data bytes to OLED address 0x3C.
  - Return ACK and raw-write status frames over CAN.
serial_log:
  - hello node=<id> type=I2C fw=<version> cap=<flags>
  - oled probe addr=0x3C result=OK
  - oled write chunk=<n> result=OK
  - oled display Hello World OK
state_machine:
  - wait_gateway_hello
  - probe_oled
  - initialize_oled
  - write_test_text
  - warn_on_ack_timeout
failure_behavior:
  - Missing HELLO blocks OLED commands.
  - Failed probe reports ADDR_NOT_FOUND and does not send display writes.
  - Failed write marks the OLED update FAILED and leaves retry policy to the caller.
must_not_include:
  - ESP32 direct OLED I2C driver calls.
  - MPU6050, motor, servo, TOF, WiFi, Bluetooth, or local UART behavior.
compile_command: .\tools\build.ps1 examples_ch32/oled_ch32_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
