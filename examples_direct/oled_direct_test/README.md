# oled_direct_test

ESP32-WROOM 直连 SSD1315（128x64、I2C 地址 `0x3C`）的最小测试例程。

## 接线

- GPIO21 -> SDA
- GPIO22 -> SCL
- GND -> GND
- 电源电压按实际 OLED 模块要求连接

## 运行现象

初始化成功后，OLED 显示：

- `你好`
- `显示正常`
- 运行秒数

屏幕每秒更新一次。串口在初始化时输出一次完整状态，运行期间每三秒输出一次精简状态。

驱动只内置例程需要的少量 16x16 汉字点阵，不包含完整中文字库，因此资源占用较小。

## 编译

```powershell
.\tools\build.ps1 examples_direct/oled_direct_test esp32 my_board_esp32wroom
```

也可以在导出 ESP-IDF 环境后进入例程目录执行：

```powershell
idf.py -D MOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -p COMx -D MOCE_BOARD=my_board_esp32wroom build flash monitor
```

## 典型日志

初始化成功：

```text
oled init result=OK
runtime state=READY addr=0x3C i2c=1 device=1 init=1 display=1 ...
```

接线、供电或地址错误：

```text
oled init result=ADDR_NOT_FOUND
retry in 3000 ms; check 3.3V/GND/SDA/SCL and address 0x3C
```
