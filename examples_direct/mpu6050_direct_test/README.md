# mpu6050_direct_test

MPU-6050 direct-I2C minimum example for ESP32-WROOM.

- Supply: `3V3`
- I2C address: `0x68`
- I2C clock: `400 kHz`
- Board pins: SDA GPIO21, SCL GPIO22
- Startup: keep the module stationary during the 50-sample gyro calibration;
  calibration has a 2 s total deadline and fails early after three consecutive
  communication failures
- Calculation rate: `100 Hz`
- Serial output rate: `5 Hz` on UART0
- Output: calculated `roll`, `pitch`, and relative `yaw` angles in degrees, plus
  `accel_fix=0/1` indicating whether this update used gravity correction
- Limitation: without a magnetometer, yaw is relative and drifts over time

After initialization succeeds, the example calculates orientation at 100 Hz
and prints summaries at 5 Hz for 60 seconds. It then deinitializes the driver
and returns. It never prints raw register values as the target measurement.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_direct/mpu6050_direct_test esp32 my_board_esp32wroom
```
