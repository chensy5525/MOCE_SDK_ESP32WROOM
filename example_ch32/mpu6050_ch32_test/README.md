# mpu6050_ch32_test

MPU-6050 minimum example through the CH32 dynamic CAN-to-I2C gateway.

- MPU-6050 supply: `3V3`
- Downstream I2C: address `0x68`, speed `400 kHz`
- CAN bitrate: `500 kbit/s`
- Discovery: F0/F1/F2 dynamic assignment and F2-confirmed stable node table
- Rediscovery: incremental merge by `device_type + token`
- Startup: keep the module stationary during 200-sample gyro calibration
- Calculation/output: `100 Hz` calculation, UART0 angles at `5 Hz`
- Duration: 100 lines per discovered MPU-6050 (about 20 seconds)
- Limitation: yaw is relative and drifts because there is no magnetometer

The driver only calls `ch32_i2c_multi_gateway_final`. Its coherent 14-byte read
uses the gateway's non-zero request ID and chunk/DONE completion. No raw
register values are printed.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_ch32/mpu6050_ch32_test esp32 my_board_esp32wroom
```
