# VL53L0X direct test

ESP32 通过 I2C0（SDA GPIO21、SCL GPIO22）直连 VL53L0X。串口每 200 ms 输出一次距离；距离超过 2000 mm 时输出 `OUT RANGE`。
