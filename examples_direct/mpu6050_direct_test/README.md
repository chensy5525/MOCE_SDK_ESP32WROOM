# mpu6050_direct_test

ESP32-WROOM direct I2C MPU6050 test.

This example does not use CAN or CH32. The MPU6050 is connected directly to
the ESP32-WROOM I2C0 header from the schematic:

- SDA: `I2C0_SDA` / GPIO21
- SCL: `I2C0_SCL` / GPIO22
- VCC: 3V3
- GND: GND
- MPU6050 AD0 is tied to GND, so the 7-bit I2C address is `0x68`.

The example wakes the MPU6050, configures +-2g accelerometer and +-250dps
gyro ranges, then prints acceleration, gyro, temperature, and tilt angle every
500 ms.

Build:

```powershell
.\tools\build.ps1 examples_direct/mpu6050_direct_test esp32 my_board_esp32wroom
```

Flash:

```powershell
.\tools\flash.ps1 examples_direct/mpu6050_direct_test COM5 esp32 my_board_esp32wroom
```
