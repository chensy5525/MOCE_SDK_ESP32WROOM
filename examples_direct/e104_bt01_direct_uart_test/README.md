# E104-BT01 direct UART test

## 接线

```text
E104-BT01 CN1.1 VCC    -> ESP32 3.3V
E104-BT01 CN1.2 MCU TX -> ESP32 GPIO17 TX
E104-BT01 CN1.3 MCU RX -> ESP32 GPIO16 RX
E104-BT01 CN1.4 GND    -> ESP32 GND
```

模块必须处于唤醒状态。首次 AT 探测应在 BLE 未连接时进行，串口配置为出厂值 19200 8N1。

## 构建

在已初始化 ESP-IDF 环境的 PowerShell 中执行：

```powershell
idf.py -C 'D:\Desktop\Firmware\ESP\zsan\examples_direct\e104_bt01_direct_uart_test' build
```

例程先发送不带 CR/LF 的 `AT` 和 `AT+BAUD?`。探测通过后，用手机连接 `E104-BT01`：

- 订阅 FFF1，接收 ESP32 周期发送的 19 Byte `ESP32-BLE-UART-TEST`。
- 向 FFF2 写入不超过 20 Byte 的数据，ESP32 串口日志输出长度和十六进制内容。

本例程只用于单模块验收，不包含烧录命令，也不自动修改模块持久化参数。
