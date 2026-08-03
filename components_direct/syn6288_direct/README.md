# SYN6288 direct driver

ESP32-WROOM 通过 UART 直接控制 SYN6288，供最小单模块例程使用。

- TX：GPIO17
- RX：GPIO16
- UART：9600 baud，8N1，标准 TTL 电平
- `syn6288_direct_init()`：初始化 UART
- `syn6288_direct_speak_test()`：播报“测试”
