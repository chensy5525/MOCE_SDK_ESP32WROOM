# mpu6050_direct_i2c

ESP32-WROOM direct I2C driver for MPU6050.

## Defaults

- SDA: `BOARD_I2C_SDA_GPIO` / GPIO21
- SCL: `BOARD_I2C_SCL_GPIO` / GPIO22
- I2C address: `0x68`, with `0x69` supported when AD0 is high
- I2C speed: 100 kHz
- Accelerometer range: +-2g
- Gyroscope range: +-250 dps

## Public API

- `mpu6050_direct_default_config()`
- `mpu6050_direct_detect()`
- `mpu6050_direct_scan()`
- `mpu6050_direct_init()`
- `mpu6050_direct_read_raw()`
- `mpu6050_direct_read_sample()`
