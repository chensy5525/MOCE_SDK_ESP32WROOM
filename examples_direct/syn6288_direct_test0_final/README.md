# SYN6288 direct test

ESP32-WROOM 通过 UART 直连 SYN6288，上电等待 3 秒后，每隔 2 秒播报一次“测试”。

## 对应驱动

`components_esp32wroom/syn6288_direct_final`

## 接线

- GPIO17（TX）连接 SYN6288 RX
- GPIO16（RX）连接 SYN6288 TX
- GND 共地
- UART：9600 baud，8N1，标准 TTL 电平

## 编译

```powershell
.\tools\build.ps1 examples_direct/syn6288_direct_test0_final esp32 my_board_esp32wroom
```
