# syn6288_ch32_test

ESP32-WROOM 通过 CAN 动态发现并分配 CH32 UART 网关节点 ID，随后由 CH32
USART1 PA9 以 9600 8N1 向 SYN6288E 发送语音帧。例程每隔 1 秒播报一次
“危险”。

驱动只保存发现层提供的稳定节点引用，不执行发现，也不写死 `node_id`。
START ACK 成功后才发送 DATA 分片，并校验 COMPLETE ACK 的处理长度。

```powershell
.\tools\build.ps1 examples_ch32/syn6288_ch32_test esp32 my_board_esp32wroom
```
