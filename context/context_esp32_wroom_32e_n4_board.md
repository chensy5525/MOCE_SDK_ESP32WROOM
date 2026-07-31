###### 模块信息:
名称: ESP32-WROOM-32E-N4 主板
模块描述: 基于 ESP32-WROOM-32E-N4 的主控板，集成 Type-C 下载/供电、3.3V LDO、SN65HVD230 CAN 收发器和标准化外接接口，可作为开发套件主控与外设控制核心。
模块类型: 主控
功能标签: ESP32-WROOM-32E-N4、主控、Wi-Fi、Bluetooth、I2C、CAN/TWAI、Type-C下载
功能定义: 运行 ESP-IDF 固件，提供 GPIO、通信总线、CAN 总线收发和外设扩展连接能力。

###### 适用场景:
推荐应用: 开发套件主控、传感器采集、执行器控制、CAN 网关通信、I2C外设扩展、Wi-Fi/Bluetooth 物联网应用
不推荐应用: 5V IO 直连场景、需要 PSRAM 的大内存应用、未隔离的大电流电机/舵机直接供电场景、占用下载串口的普通外设场景

###### 硬件能力:
核心芯片: ESP32-WROOM-32E-N4
关键参数: ESP32-WROOM-32E-N4；无 PSRAM；3.0V~3.6V 模组供电；3.3V IO；Wi-Fi 802.11b/g/n；Bluetooth v4.2 BR/EDR + BLE；1 路 TWAI/CAN 控制器扇出到 10 个 CAN 标准化接口；2 个 I2C 标准化接口；1个电机信号口
通信: Wi-Fi、Bluetooth、I2C、CAN/TWAI；CAN 标准化接口共享同一 CAN 总线
控制: 外设控制
采集: 无采集功能
输出: LED=GPIO12、CANH/CANL、I2C0、电机控制信号

###### 硬件接口:
接口类型: Type-C、XT30 电源输入、CAN 标准化接口、I2C 标准化接口、电机信号口
数量: Type-C 1 个；XT30 电源输入 1 个；CAN 标准化接口 10 个；I2C 标准化接口 2 个；电机信号口 1 个
通信协议: USB-UART、I2C、CAN/TWAI
工作模式:
 - 主机
引脚定义: I2C0 SDA=GPIO21、SCL=GPIO22；CAN_TX=GPIO5、CAN_RX=GPIO4，经 SN65HVD230 输出 CANH/CANL，10 个 CAN 标准化接口共享同一 CAN 总线；电机信号A口的标准化接口对应PWM1=GPIO32、PWM2=GPIO33、PWMA=GPIO25、PWMB=GPIO26
硬件地址：无固定硬件地址；Wi-Fi/Bluetooth MAC 地址由 ESP32 模组提供

###### 供电信息:
工作电压: ESP32 模组供电 3.0V~3.6V，典型 3.3V；主板由 Type-C VBUS / 外部 VIN 输入，经 AMS1117-3.3S 生成 VDD 3.3V；外接接口另有 +5V/V5+ 和 VDD 网络
电流需求: ESP32-WROOM-32E 数据手册建议外部电源供电能力不低于 0.5A；板载自耗需覆盖 ESP32 Wi-Fi TX 峰值约 379mA、USB-UART/CAN 收发器/LDO 静态电流和 LED；外设接口电流需另行按实际接入模块累加
最大功耗: 板载自耗峰值保守估算约 1.4W；5V 经 AMS1117 线性降压时，3.3V 侧 430mA 会额外产生约 0.73W LDO 热耗；10 个 CAN 标准化接口、2 个 I2C 标准化接口、2 个电机信号口、1 个舵机信号口、1 个串口若给外设供电，功耗按外设模块另计
供电注意事项: ESP32 模组不能直接接 5V；外部输入经保险丝 SMD0603-075-6 和 SMBJ6.5CA 保护后进入 +5V；10 个 CAN 标准化接口和 2 个 I2C 标准化接口若同时带 3V3 外设，应按每个外设的峰值电流做电源预算，不能默认全部由 AMS1117 长时间满载供电；电机口和舵机口默认只按信号口处理，电机/舵机动力电源应独立供电并共地。

###### 系统资源占用:
GPIO占用: LED=GPIO12；I2C0 SDA/SCL=GPIO21/GPIO22；CAN TX/RX=GPIO5/GPIO4；PWM_B 输出脚=GPIO32/GPIO33/GPIO25/GPIO26
通信资源占用:
I2C: I2C_NUM_0，SDA GPIO21，SCL GPIO22，400kHz，timeout 1000ms，glitch ignore 7，internal pull-up enabled
CAN: 1 路 ESP32 TWAI/CAN 控制器，TX GPIO5，RX GPIO4，经 SN65HVD230 到 CANH/CANL，并扇出到 10 个 CAN 标准化接口
中断资源: GPIO 中断按业务占用
定时器资源: LED PWM 使用 LEDC high speed timer0/channel0，10-bit，5000Hz；电机 PWM 默认使用 timer2/channel3/channel4，10-bit，20000Hz
DMA需求: 暂无

###### 使用限制:
硬件限制: ESP32-WROOM-32E-N4 为 4MB Quad SPI Flash、无 PSRAM；
电气限制: IO 电平为 3.3V；5V 外设需确认电平转换或 3.3V 兼容；CAN 总线需要正确终端电阻
通信限制: 使用 my_board_esp32wroom profile；目标芯片为 esp32；sdkconfig.defaults 配置 4MB Flash、single app partition、INFO 日志
不兼容情况: 不兼容 5V IO 直接灌入 ESP32；不兼容占用 GPIO6~GPIO11 的外设设计；不兼容未规划资源冲突的多 PWM/多总线同时使用；普通外设不应占用 GPIO0 下载按键功能
兼容性: 可连接模块: I2C/CAN/PWM外设 - 推荐搭配主控: 无；该模块本身是主控板 - 推荐搭配模块: ESP32 转接板、OLED、VL53L0X、MPU6050、舵机/电机驱动模块 - 替代模块: ESP32-S3 主板

###### 软件生态:
驱动支持: ESP-IDF；最终 Board Profile 为 boards/my_board_esp32wroom
驱动依赖: bsp_board、bsp_i2c、bsp_uart、bsp_pwm、bsp_gpio、ESP-IDF GPIO/UART/I2C/SPI/LEDC/TWAI
示例工程: 见examples_ch32下各文件

###### 工程元数据:
成本区间: 15-25元
优点: 主控能力完整，接口类型丰富，板上集成 Type-C 自动下载、CAN 收发器和 3.3V LDO，适合开发套件统一主控
缺点: 资源复用较多，启动绑带脚和下载串口需要避让；无 PSRAM；大电流外设需要独立供电规划；PWM_A 命名易误导但对应输入专用脚
文件资源: 
数据手册: 
原理图: 
3D模型: 
