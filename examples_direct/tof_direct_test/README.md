# tof_direct_test

ESP32-WROOM 通过 I2C 直连 VL53L0X 激光测距模块的最小测试例程，不使用 CH32。

## 接线

- ESP32 GPIO21 SDA -> VL53L0X SDA
- ESP32 GPIO22 SCL -> VL53L0X SCL
- ESP32 GND -> VL53L0X GND
- 按实际模块要求连接有效电源

## 上电逻辑

1. 通过 `vl53l0x_direct_init()` 初始化 I2C。
2. 探测地址 `0x29`；无应答时扫描 `0x03` 到 `0x77` 并打印发现的地址。
3. 读取 `0xC0` 型号寄存器，期望值为 `0xEE`。
4. 执行 VL53L0X 初始化序列。
5. 每 500 ms 读取一次距离，并区分有效距离、超量程和通信失败。

## 编译

```powershell
.\tools\build.ps1 examples_direct/tof_direct_test esp32 my_board_esp32wroom
```

也可以在导出 ESP-IDF 环境后进入例程目录执行：

```powershell
idf.py -D MOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -p COMx -D MOCE_BOARD=my_board_esp32wroom build flash monitor
```

## 典型日志

```text
==== ESP32-WROOM tof_direct_test ====
i2c initialized, probing VL53L0X
tof init result=OK
TOF_DATA ready=1 valid=1 sensor_alive=1 addr=0x29 mm=<number> raw=<number> result=OK
```

超量程不表示模块损坏：

```text
TOF_DATA ready=1 valid=0 sensor_alive=1 addr=0x29 raw=<number> result=OUT_OF_RANGE
```

## 失败含义

- `ADDR_NOT_FOUND`：`0x29` 没有应答，随后会输出 I2C 扫描结果。
- `MODEL_ID_READ_FAIL`：地址有应答，但读取 `0xC0` 失败。
- `MODEL_ID_MISMATCH`：型号寄存器不是 `0xEE`。
- `CONFIG_FAIL`：初始化序列写入失败。
- `COMM_FAIL`：运行期 I2C 通信失败。
- `MEASURE_TIMEOUT`：测距结果在超时前没有就绪。
