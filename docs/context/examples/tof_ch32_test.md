# TOF CH32 Test Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: tof_ch32_test
purpose: Read VL53L0X laser distance measurements through a CH32 I2C gateway.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_vl53l0x_gateway
protocol_contracts:
  - ch32_vl53l0x_gateway
user_goal:
  - ESP32-WROOM reads distance from a VL53L0X connected to CH32.
required_behavior:
  - ESP32 waits for the CH32 I2C bridge HELLO frame.
  - ESP32 probes VL53L0X address 0x29 through CH32.
  - ESP32 reads and validates model ID 0xEE.
  - ESP32 sends the minimal VL53L0X configuration sequence through CH32.
  - ESP32 prints distance in millimeters every 500 ms after initialization succeeds.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Detect the CH32 I2C bridge node from HELLO frames.
  - Encode documented probe, write-register, and read-register commands.
  - Distinguish communication failure from out-of-range distance status.
  - Log initialization and runtime measurement result codes.
ch32_responsibilities:
  - Own the downstream I2C bus.
  - Perform VL53L0X I2C probe, register writes, and register reads.
  - Return transaction results and read bytes to ESP32 over CAN.
serial_log:
  - gateway wait result=OK
  - tof init result=OK
  - tof distance=<number> mm raw=<number> result=OK
  - tof distance out_of_range raw=<number> result=OUT_OF_RANGE
state_machine:
  - wait_gateway_hello
  - probe_tof
  - validate_model_id
  - configure_tof
  - read_distance_loop
  - warn_on_comm_fail
failure_behavior:
  - Missing HELLO blocks TOF initialization.
  - Address, model ID, or configuration failure stops the measurement loop and logs the failing stage.
  - Runtime communication failure marks the sample invalid.
  - Out-of-range is logged as a valid sensor status, not as a broken module.
must_not_include:
  - ESP32 direct VL53L0X I2C driver calls.
  - OLED, MPU6050, motor, servo, VC02, SYN6288, WiFi, or Bluetooth behavior.
compile_command: .\tools\build.ps1 example/tof_ch32_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
