# SYN6288 CH32 Gateway Test Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: syn6288_ch32_gateway_test
purpose: Send one preassembled SYN6288E speech frame through a CH32 UART gateway.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_syn6288_gateway
protocol_contracts:
  - ch32_syn6288_gateway
user_goal:
  - ESP32-WROOM asks CH32 to play one SYN6288E voice prompt.
required_behavior:
  - ESP32 sends a transfer START frame with total length and CRC.
  - ESP32 fragments the opaque SYN6288E UART frame into CAN DATA frames.
  - ESP32 checks START and completion ACKs from CH32.
  - CH32 forwards the reconstructed bytes over USART1 PA9.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Use the documented 0x430 START and 0x431 DATA frames.
  - Calculate or provide CRC-16/CCITT-FALSE for the opaque frame.
  - Send fragments in sequence and stop on failed ACK/status.
  - Log transfer ID, byte count, fragment count, and ACK detail.
ch32_responsibilities:
  - Reassemble one active SYN6288E transfer.
  - Validate transfer ID, sequence, total length, and CRC.
  - Forward bytes unchanged on CH32 UART at 9600 8N1.
  - Return gateway ACK frames on CAN ID 0x500.
serial_log:
  - transfer=<id> length=<bytes> fragments=<count> crc=<crc16>
  - ACK phase=<phase> source=<can_id> result=<result> transfer=<id> detail=<detail>
  - one-shot SYN6288E transfer completed successfully
state_machine:
  - start_transfer
  - send_fragments
  - wait_completion_ack
  - stop_after_one_transfer
failure_behavior:
  - Failed START ACK prevents DATA frames from being sent.
  - Missing completion ACK logs FAILED and does not automatically replay speech.
  - Sequence or CRC failure leaves CH32 to discard the transfer.
must_not_include:
  - ESP32 direct SYN6288E UART writes.
  - OLED, MPU6050, motor, servo, TOF, WiFi, or Bluetooth behavior.
compile_command: .\tools\build.ps1 example_final/syn6288_ch32_gateway_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
