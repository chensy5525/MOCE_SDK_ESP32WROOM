# syn6288e_direct_test

ESP32-WROOM UART1 直连 SYN6288E 的最小例程，每隔 1 秒播报一次“危险”。

- ESP32 TX GPIO17 连接 SYN6288E RXD。
- ESP32 RX GPIO16 连接 SYN6288E TXD（如果模块引出 TXD）。
- 共地，串口参数为 9600 8N1。
- UART0 仅用于烧录与日志，驱动不会使用 UART0。

```powershell
.\tools\build.ps1 examples_direct/syn6288e_direct_test esp32 my_board_esp32wroom
```

`SYN6288E_TX route=DIRECT_UART result=OK` 只表示完整语音帧已经从 UART
发送完成。实际发声还需要通过模块供电、接线、功放和扬声器现象确认。
