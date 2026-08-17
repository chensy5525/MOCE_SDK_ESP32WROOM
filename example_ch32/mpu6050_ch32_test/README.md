# mpu6050_ch32_test

MPU-6050 minimum example through the CH32 dynamic CAN-to-I2C gateway.

- MPU-6050 supply: `3V3`
- Downstream I2C: address `0x68`, speed `400 kHz`
- CAN bitrate: `500 kbit/s`
- Discovery: F0/F1/F2 dynamic assignment and F2-confirmed stable node table
- Rediscovery: every 10 seconds only while no MPU6050 has been bound
- Startup: keep the module stationary during the bounded 50-sample gyro calibration
- Calculation/output: `100 Hz` calculation, UART0 angles at `5 Hz`
- Instance policy: try confirmed candidates in order and retain the first MPU6050 whose live probe and initialization succeed
- Duration: calculate at 100 Hz and print at 5 Hz for 60 seconds after the
  first module binds successfully, then deinitialize and return
- Limitation: yaw is relative and drifts because there is no magnetometer

The driver only calls `ch32_i2c_multi_gateway_final`. Its coherent 14-byte read
uses the gateway's non-zero request ID and chunk/DONE completion. No raw
register values are printed.

Build from the repository root:

```powershell
.\tools\build.ps1 examples_ch32/mpu6050_ch32_test esp32 my_board_esp32wroom
```
