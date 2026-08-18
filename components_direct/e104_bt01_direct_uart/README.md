# E104-BT01 direct UART driver

ESP32-WROOM 通过主板 UART1 直接控制第二批 E104-BT01 蓝牙模块板。

## 硬件绑定

| E104-BT01 板 CN1 | ESP32-WROOM 主板 |
|---|---|
| 1 VCC | 3.3 V |
| 2 MCU TX / BRX | GPIO17 / UART1 TX |
| 3 MCU RX / BTX | GPIO16 / UART1 RX |
| 4 GND | GND |

- 电平：3.3 V TTL，禁止接 5 V UART。
- 出厂串口：19200 baud、8N1、无流控。
- UART1 是排他资源，不能同时用于 SYN6288 等其他串口模块。

## 能力

- 有界超时的二进制 UART 读写。
- AT 请求/响应事务；发送命令不追加 CR/LF。
- 识别 `+OK\r\n`、`+OK=...\r\n`、`+ERR=NUM\r\n`。

CN1 没有引出 LINK、DATA、MOD、WKP、RESET_N，因此本驱动不提供连接检测、软件模式切换、低功耗管理或硬件复位。

## 使用约束

AT 指令前必须保证模块处于唤醒和配置状态。未连接时模块自动处于配置状态；连接后由板载 MOD 开关决定配置/透传状态。状态打印与透传共用 UART，基础驱动不会自动过滤 `STA:*` 数据。

驱动与 `bsp_uart` 使用单任务、串行访问契约。调用方不得让 AT 事务与透传读写并发执行；如系统需要多任务共享，应由上层统一任务或互斥锁仲裁 UART1 所有权。
