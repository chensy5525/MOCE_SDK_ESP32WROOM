# mpu6050_direct_test

MPU-6050 direct-I2C minimum example for ESP32-WROOM.

- Supply: `3V3`
- I2C address: `0x68`
- I2C clock: `400 kHz`
- Board pins: SDA GPIO21, SCL GPIO22
- Startup: keep the module stationary during the 200-sample gyro calibration
- Calculation rate: `100 Hz`
- Serial output rate: `5 Hz` on UART0
- Output: calculated `roll`, `pitch`, and relative `yaw` angles in degrees
- Limitation: without a magnetometer, yaw is relative and drifts over time

The example prints 100 consecutive orientation lines (about 20 seconds), deinitializes the driver,
and then waits. It never prints raw register values as the target measurement.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_direct/mpu6050_direct_test esp32 my_board_esp32wroom
```
