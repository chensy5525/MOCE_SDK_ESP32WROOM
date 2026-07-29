# MPU6050 CH32 Gateway Test Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: mpu6050_ch32_test
purpose: Read MPU6050 acceleration, gyroscope, and temperature values through a CH32 I2C gateway.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_mpu6050_gateway
protocol_contracts:
  - ch32_mpu6050_gateway
user_goal:
  - ESP32-WROOM reads MPU6050 data through CH32 over CAN.
required_behavior:
  - ESP32 waits for the CH32 I2C gateway HELLO frame.
  - ESP32 probes MPU6050 address 0x68 through CH32.
  - ESP32 initializes MPU6050 registers through documented CH32 I2C commands.
  - ESP32 periodically requests and prints acceleration, gyroscope, and temperature data.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Detect the CH32 I2C gateway node from HELLO frames.
  - Encode documented probe, register write, and register read requests.
  - Reassemble returned register data chunks and validate ACK/status.
  - Print scaled MPU6050 values and warn on timeout or bad status.
ch32_responsibilities:
  - Own the downstream I2C bus.
  - Probe address 0x68 and perform MPU6050 register reads/writes.
  - Return ACK and read-data status frames over CAN.
serial_log:
  - hello node=<id> type=I2C fw=<version> cap=<flags>
  - mpu6050 whoami=0x68 init=OK
  - mpu6050 accel=<x,y,z> gyro=<x,y,z> temp=<celsius>
state_machine:
  - wait_gateway_hello
  - probe_mpu6050
  - configure_mpu6050
  - read_and_log_samples
  - warn_on_timeout
failure_behavior:
  - Missing HELLO blocks MPU6050 initialization.
  - Failed probe reports ADDR_NOT_FOUND and keeps retrying only at the recipe interval.
  - Failed register read marks the sample invalid and does not reuse stale values as fresh data.
must_not_include:
  - ESP32 direct I2C MPU6050 initialization.
  - OLED, motor, servo, TOF, WiFi, Bluetooth, or local UART behavior.
compile_command: .\tools\build.ps1 example/mpu6050_ch32_test esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
