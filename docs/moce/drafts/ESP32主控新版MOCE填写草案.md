# ESP32 主控新版 MOCE Revision 填写草案（网页 Schema 3.1.0 对齐版）

## 0. 文档用途与边界

- 目标模块：ESP32 主控板本体，不含独立的 ESP32 新版转接板。
- 目标 Schema：MOCE Schema 3.1.0。
- 主依据：`SCH_ESP32-WROOM-32_2026-08-14.pdf`，原理图版本 V1.0。
- 辅助依据：ESP32-WROOM-32E-N4 芯片资料、当前 ESP32 固件仓库接口审查、新版转接板原理图。
- 本文是新 Revision 的录入草案，不代表已完成线上发布、编译验证或硬件验证。
- 所有电流能力、成本、量产状态和未核对的接口标准 ID 均不得沿用旧 Revision 的数值。

### 0.1 已读取的网页规范（2026-08-17）

当前网页前端为 Schema `3.1.0`，填写流程固定为四步：

1. `基础说明`：模块身份、人类说明、适用场景、限制、资料成熟度和允许用途。
2. `资料来源`：原理图、制造商文档、驱动代码、实测记录或人工说明。
3. `工程候选`：能力、统一接口、工作方式、接口角色、接口关系、供电关系和工程事实。
4. `审核发布`：为每条关键记录关联来源并执行人工确认，然后由系统发布新 Revision。

网页字段的关键枚举如下：

| 字段 | 网页允许值 |
|---|---|
| 资料成熟度 | `信息录入中`、`资料核验中`、`资料已核验`、`内部推荐使用` |
| 能力类型 | `控制`、`感知`、`执行`、`通讯`、`转换/转发`、`供电`、`显示`、`计算`、`存储`、`机械`、`其它` |
| 接口方向 | `输入到本模块`、`从本模块输出`、`双向`、`无方向/被动连接` |
| 来源类型 | `制造商文档`、`电路原理图`、`驱动代码`、`实测记录`、`人工说明` |
| 工程事实类别 | `electrical`、`mechanical`、`performance`、`control`、`environment`、`commercial`、`software`、`other` |
| 关键性 | `advisory`、`engineering_critical`、`safety_critical` |
| 验证状态 | `待审核`、`人工确认`、`文档验证`、`实测验证`、`存在冲突`、`已拒绝` |

录入新记录时网页会先设为“待审核”。在最后一步绑定来源并点击“人工确认”后，记录才算通过人工审核。

当前接口标准目录和模块 API 需要登录态；本次只读连接未继承浏览器登录 Cookie，因此无法取得现有接口标准的 UUID。本文给出应选择的标准名称，稍后填写时必须在网页下拉框中选择真实条目，不能手写伪造 UUID。

### 0.2 网页就绪度规则的实际含义

- `器件选型`：要求模块已发布、允许原型开发、至少有一项能力，并且能力及其参数事实已审核。
- `连接拓扑`：在器件选型基础上，要求接口标准已启用，接口/供电/接口关系已审核，所有启用接口都说明连接必要性，必要工作条件可被本模块接口满足。
- `固件生成`：当前前端代码直接复用了连接拓扑的条件，没有额外检查编译、API、commit 或 HIL 证据。
- `量产判断`：还要求允许量产，并且所有非 advisory 事实达到文档验证或实测验证。

因此，网页显示“固件生成可用”只能说明网页结构化数据通过了当前规则，不能替代 ESP-IDF 编译和上板验证。

---

## 1. Revision 策略

建议选择“编辑新 Revision”，保留模块代码 `MOCE-ESP`，不要覆盖或永久删除 Revision 50。

| 字段 | 建议填写 |
|---|---|
| 模块名称 | `ESP32 Main Controller Board`（保持既有模块身份；芯片型号写入说明和工程事实） |
| 模块代码 | `MOCE-ESP` |
| Schema | `3.1.0` |
| 硬件版本 | `V1.0` |
| 模块类型 | `主控 PCBA` |
| 集成层级 | `基元` |
| 制造商 | `待确认，不填写` |
| 数据状态 | `草稿/待审核`；完成关键事实复核后再发布 |
| 允许用途 | 仅勾选 `原型开发` |
| 量产用途 | 不勾选 |

### 1.1 不能直接沿用 Revision 50 的内容

1. 主控板与转接板的接口数量曾被合并；新版主控本体只有一个 CAN 外部插座。
2. 旧 Revision 的 `650 mA`、`100 mA`、`102 mA` 等供电数值缺少新版 BOM、热设计和实测依据。
3. 旧 Revision 的 `board_init`、`board_can_init`、`board_adc_read` 等 API 在当前公共 BSP 中没有找到对应实现。
4. 旧成本 `35–80 RMB` 没有新版 BOM/采购记录支持。
5. 新原理图显示 LED 为 `GPIO23`；当前仓库板级配置仍为 `GPIO12`。
6. 主控板与新版转接板都画有 120 Ω CAN 终端电阻，是否同时装配必须按总线物理位置决定。

---

## 2. 人类说明

### 2.1 Human Summary（直接填写）

```text
An ESP32-WROOM-32E-N4 main controller board integrating 2.4 GHz Wi-Fi/Bluetooth connectivity, USB Type-C programming and debugging, one onboard CAN interface, one UART interface, one I2C interface, I2S, four ADC channels, PWM outputs, external motor-driver control signals, and board-to-board expansion headers.
```

### 2.2 Details（直接填写）

```text
This V1.0 main controller board is built around the ESP32-WROOM-32E-N4 module. A USB Type-C connector, CH340X USB-to-UART bridge, and automatic boot circuitry provide UART0 firmware download and serial debugging. An onboard SN65HVD230DR transceiver converts ESP32 TWAI signals on GPIO5/GPIO4 to a differential CANH/CANL interface through an ACT1210-510-2P common-mode choke, with an onboard 120-ohm termination resistor. Dedicated GH1.25-4P connectors expose one 3.3 V UART1 interface, one I2C0 controller interface, and one CAN interface. An 18-pin signal header exposes CAN, I2S, ADC, PWM, motor-driver control, and one general-purpose I/O signal. A 12-pin power header exposes repeated 3.3 V, GND, and 5 V contacts for the matching adapter board. The board accepts nominal 5 V from USB Type-C or XT30 and generates the 3.3 V rail through an AMS1117-3.3S regulator. The Type-C and XT30 inputs share the 5 V rail; the schematic does not establish protected simultaneous-input operation. External motors, motor power stages, sensors, microphones, servo loads, and the separate ESP32 adapter board are not part of this module.
```

### 2.3 典型用途

建议录入以下 4 条：

1. `Prototype controller for CAN-connected sensor and actuator nodes.`
2. `Wi-Fi/Bluetooth gateway with local UART, I2C, I2S, ADC, PWM, and GPIO resources.`
3. `Main controller for the matching ESP32 adapter board through the 18-pin signal and 12-pin power headers.`
4. `Firmware development, serial debugging, and hardware-in-the-loop validation through USB Type-C.`

### 2.4 限制条件

建议逐条录入：

1. `Prototype use only; production readiness has not been established.`
2. `ESP32 GPIO uses 3.3 V logic and is not 5 V tolerant.`
3. `The ESP32-WROOM-32E-N4 variant has 4 MB flash and no PSRAM.`
4. `GPIO0, GPIO2, GPIO5, GPIO12, and GPIO15 are boot-strapping pins; external circuits must not force invalid levels during reset.`
5. `GPIO34, GPIO35, SENSOR_VP/GPIO36, and SENSOR_VN/GPIO39 are input-only analog-capable pins.`
6. `The main board provides one physical CAN connector. Ten parallel CAN connectors belong to the separate adapter board.`
7. `Both the main board and the matching adapter schematic include a 120-ohm CAN termination resistor. Only bus-end termination shall be populated.`
8. `USB Type-C and XT30 connect to the same 5 V rail. No ideal-diode OR-ing or reverse-current isolation is established by the schematic.`
9. `Available 3.3 V and 5 V output current is not verified. Do not enter the old 650 mA or 100 mA values without BOM, thermal, and load-test evidence.`
10. `The Type-C port is a USB 2.0 programming/debug interface and 5 V input; no USB Power Delivery controller is shown.`
11. `Current reusable BSP coverage is incomplete for CAN/TWAI, ADC, and I2S; firmware-generation readiness must remain unavailable until the board profile and APIs are corrected and built.`

---

## 3. 模块自身能力

建议录入 11 项。`来源`优先关联新版主控原理图；软件实现类能力同时关联仓库审查记录。

### 3.1 Embedded Processing and Control

| 字段 | 内容 |
|---|---|
| 名称 | `Embedded Processing and Control` |
| kind | `control` |
| quantity | `1` |
| humanStatement | `Executes firmware control logic and coordinates onboard and external communication and I/O resources.` |
| 输入 | `Firmware commands, communication frames, digital inputs, and analog samples` |
| 输出 | `Control decisions, communication frames, GPIO states, and timed outputs` |
| 约束 | `ESP32-WROOM-32E-N4; 4 MB flash; no PSRAM` |
| 验证状态 | `人工确认` |

### 3.2 2.4 GHz Wireless Communication

| 字段 | 内容 |
|---|---|
| 名称 | `2.4 GHz Wi-Fi and Bluetooth Communication` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Provides integrated 2.4 GHz Wi-Fi and Bluetooth connectivity through the ESP32-WROOM-32E-N4 module.` |
| 输入 | `Wireless packets and firmware network data` |
| 输出 | `Wireless packets and connection status` |
| 约束 | `Antenna clearance and RF layout must follow the module requirements.` |
| 验证状态 | `人工确认` |

### 3.3 USB Programming and Debugging

| 字段 | 内容 |
|---|---|
| 名称 | `USB Programming and Debugging` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Converts USB 2.0 data to ESP32 UART0 and controls EN/IO0 for automatic firmware download.` |
| 输入 | `USB host data and 5 V VBUS` |
| 输出 | `UART0 data, boot-control signals, and serial debug data` |
| 约束 | `CH340X bridge; programming uses GPIO1/TXD0, GPIO3/RXD0, EN, and GPIO0.` |
| 验证状态 | `人工确认` |

### 3.4 CAN Bus Communication

| 字段 | 内容 |
|---|---|
| 名称 | `CAN Bus Communication` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Converts ESP32 TWAI transmit/receive signals to a 3.3 V differential CAN physical interface.` |
| 输入 | `CANH/CANL differential frames and ESP32 CAN_TX data` |
| 输出 | `ESP32 CAN_RX data and CANH/CANL differential frames` |
| 约束 | `GPIO5=CAN_TX; GPIO4=CAN_RX; SN65HVD230DR; ACT1210-510-2P; onboard 120-ohm termination.` |
| 验证状态 | `人工确认` |

### 3.5 UART Peripheral Communication

| 字段 | 内容 |
|---|---|
| 名称 | `UART Peripheral Communication` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Provides one external 3.3 V TTL UART1 endpoint.` |
| 输入 | `RXD1 on GPIO16` |
| 输出 | `TXD1 on GPIO17` |
| 约束 | `External connector also exposes 3.3 V and GND; signal pins are not RS-232 tolerant.` |
| 验证状态 | `人工确认` |

### 3.6 I2C Controller Communication

| 字段 | 内容 |
|---|---|
| 名称 | `I2C Controller Communication` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Provides one external I2C controller endpoint for sensors and peripherals.` |
| 输入 | `I2C0_SDA data and peripheral responses` |
| 输出 | `I2C0_SCL clock and I2C0_SDA controller data` |
| 约束 | `GPIO21=I2C0_SDA; GPIO22=I2C0_SCL; 3.3 V logic.` |
| 验证状态 | `人工确认` |

### 3.7 I2S Signal Interface

| 字段 | 内容 |
|---|---|
| 名称 | `I2S Signal Interface` |
| kind | `communication` |
| quantity | `1` |
| humanStatement | `Exposes I2S word-select, serial-clock, and serial-data signals through the signal header.` |
| 输入 | `I2S_SD on GPIO2 when used with an external digital microphone` |
| 输出 | `I2S_WS on GPIO19 and I2S_SCK on GPIO18` |
| 约束 | `GPIO2 is a boot-strapping pin; external loading must preserve valid reset behavior.` |
| 验证状态 | `原理图确认；软件支持待确认` |

### 3.8 Analog Acquisition

| 字段 | 内容 |
|---|---|
| 名称 | `Analog Acquisition` |
| kind | `sensing` |
| quantity | `4` |
| humanStatement | `Exposes four input-only ESP32 ADC-capable channels through the signal header.` |
| 输入 | `ADC1=SENSOR_VP/GPIO36; ADC2=SENSOR_VN/GPIO39; ADC3=GPIO34; ADC4=GPIO35` |
| 输出 | `Digitized analog samples` |
| 约束 | `Input range and attenuation are firmware-dependent; no 5 V tolerance or external signal conditioning is established.` |
| 验证状态 | `原理图确认；量程与软件支持待确认` |

### 3.9 PWM Output

| 字段 | 内容 |
|---|---|
| 名称 | `PWM Output` |
| kind | `actuation` |
| quantity | `4` |
| humanStatement | `Provides four firmware-controlled PWM-capable outputs for external loads or driver inputs.` |
| 输入 | `Firmware duty-cycle and frequency commands` |
| 输出 | `PWM1/GPIO32, PWM2/GPIO33, PWMA/GPIO25, PWMB/GPIO26` |
| 约束 | `Logic-level signals only; external load drivers are required.` |
| 验证状态 | `人工确认` |

### 3.10 External Motor-Driver Control

| 字段 | 内容 |
|---|---|
| 名称 | `Dual-Channel External Motor-Driver Control` |
| kind | `actuation` |
| quantity | `2` |
| humanStatement | `Provides direction and PWM control signals for two external motor-driver channels.` |
| 输入 | `Firmware motor direction and speed commands` |
| 输出 | `Channel A: AIN1/GPIO27, AIN2/GPIO14, PWMA/GPIO25; Channel B: BIN1/GPIO12, BIN2/GPIO13, PWMB/GPIO26` |
| 约束 | `No motor power stage is included on the main board; GPIO12 is a boot-strapping pin.` |
| 验证状态 | `人工确认` |

### 3.11 Board-to-Board Expansion

| 字段 | 内容 |
|---|---|
| 名称 | `Board-to-Board Signal and Power Expansion` |
| kind | `communication`（网页没有 `connectivity` 枚举） |
| quantity | `1` |
| humanStatement | `Connects the main board to the matching adapter board through one 18-pin signal header and one 12-pin power header.` |
| 输入 | `CAN, I2S data, ADC signals, and IO15 as applicable` |
| 输出 | `I2S clocks, PWM, motor-control signals, 3.3 V, and 5 V` |
| 约束 | `Connector orientation and mating pin map must be verified against PCB assembly drawings before topology readiness is asserted.` |
| 验证状态 | `待人工复核装配方向` |

---

## 4. 统一接口

### 4.1 接口总表

| # | 网站显示名称 | 建议标准 | 方向 | 数量 | 角色/模式 | 录入状态 |
|---:|---|---|---|---:|---|---|
| 1 | `USB Type-C Programming/Power` | 现有 `USB Type-C / TYPE-C` | `bidirectional` | 1 | USB device、下载调试、5 V 取电 | 人工确认 |
| 2 | `XT30 5V Power Input` | 现有 `5V 电源输入`，连接器补充为 XT30 | `input` | 1 | 5 V 输入 | 人工确认 |
| 3 | `UART1 GH1.25-4P` | 现有 `UART` | `bidirectional` | 1 | 3.3 V TTL 通讯端点 | 人工确认 |
| 4 | `CAN GH1.25-4P` | 现有 `CAN` | `bidirectional` | 1 | 对等 CAN 节点 | 人工确认 |
| 5 | `I2C0 GH1.25-4P` | 现有 `I2C` | `bidirectional` | 1 | 固定为控制端 | 人工确认 |
| 6 | `18-Pin Signal Expansion Header` | 建议新建 `MOCE-ESP-18P-SIGNAL-V1` | `bidirectional` | 1 | 混合信号扩展 | 引脚方向待装配复核 |
| 7 | `12-Pin Power Expansion Header` | 建议新建 `MOCE-ESP-12P-POWER-V1` | `output` | 1 | 向匹配转接板提供 3.3 V/5 V/GND | 电流能力待验证 |

注意：如果统一接口目录没有 18Pin、12Pin 两个板对板标准，应先建立接口标准，不要把它们伪装成 UART、GPIO 或普通 5 V 输入。

### 4.2 外部连接器引脚

#### USB Type-C

- VBUS：5 V 输入。
- D+、D-：连接 CH340X USB-UART 桥。
- CC1、CC2：各有 5.1 kΩ 下拉。
- GND：系统地。
- 用途：供电、固件下载、串口调试。

#### XT30 5 V 输入

| Pin | 信号 |
|---:|---|
| 1 | `+5V` |
| 2 | `GND` |

#### UART1 GH1.25-4P

| Pin | 信号 | 方向（相对主控） |
|---:|---|---|
| 1 | `3V3/VDD` | 电源输出 |
| 2 | `TXD1 / GPIO17` | 输出 |
| 3 | `RXD1 / GPIO16` | 输入 |
| 4 | `GND` | 电源回路 |

#### CAN GH1.25-4P

| Pin | 信号 |
|---:|---|
| 1 | `3V3/VDD` |
| 2 | `CANL` |
| 3 | `CANH` |
| 4 | `GND` |

#### I2C0 GH1.25-4P

| Pin | 信号 |
|---:|---|
| 1 | `3V3/VDD` |
| 2 | `I2C0_SCL / GPIO22` |
| 3 | `I2C0_SDA / GPIO21` |
| 4 | `GND` |

#### 18-Pin Signal Expansion Header H1

| Pin | 信号 | ESP32 GPIO/说明 |
|---:|---|---|
| 1 | `CANH` | CAN 总线高线 |
| 2 | `CANL` | CAN 总线低线 |
| 3 | `I2S_WS` | GPIO19 |
| 4 | `I2S_SCK` | GPIO18 |
| 5 | `I2S_SD` | GPIO2 |
| 6 | `IO15` | GPIO15 |
| 7 | `BIN1` | GPIO12 |
| 8 | `BIN2` | GPIO13 |
| 9 | `AIN1` | GPIO27 |
| 10 | `AIN2` | GPIO14 |
| 11 | `PWMA` | GPIO25 |
| 12 | `PWMB` | GPIO26 |
| 13 | `PWM1` | GPIO32 |
| 14 | `PWM2` | GPIO33 |
| 15 | `ADC3` | GPIO34，输入专用 |
| 16 | `ADC4` | GPIO35，输入专用 |
| 17 | `ADC1` | SENSOR_VP/GPIO36，输入专用 |
| 18 | `ADC2` | SENSOR_VN/GPIO39，输入专用 |

#### 12-Pin Power Expansion Header H2

| Pins | 信号 | 角色 |
|---|---|---|
| 1、2、3、4 | `3V3/VDD` | 向转接板供电，允许电流待验证 |
| 5、6、7、8 | `GND` | 电源回路 |
| 9、10、11、12 | `+5V` | 向转接板传递 5 V，允许电流待验证 |

### 4.3 接口协作关系

当前建议保持 `0` 条。

原因：Schema 的接口协作关系用于模块外部统一接口之间的转换关系，而 USB-UART、ESP32-TWAI 到 CAN 收发器、5 V 到 3.3 V 都属于板内链路。除非系统允许显式建立“板内逻辑端点”，否则不要用外部 UART1 接口冒充 USB 对应的 UART0，也不要建立错误的 USB Type-C → UART1 关系。

### 4.4 工作方式：正常工作

网页至少要求一个工作方式，使用默认工作方式即可：

| 字段 | 填写内容 |
|---|---|
| ID | 保留系统默认 `normal` |
| 名称 | `正常工作` |
| 描述 | `The board executes controller firmware and exposes optional communication, acquisition, control, and expansion interfaces while powered from exactly one approved 5 V input.` |
| 启用能力 | 勾选本 Revision 的全部 11 项能力 |
| 启用接口 | 勾选本 Revision 的全部 7 个接口 |
| 启用供电关系 | 勾选第 5 节的全部 7 条供电关系 |
| 接口关系 | 不添加 |

#### 当前工作方式中的接口连接要求

| 接口 | 必要性 | 备选接口组 | 用途 |
|---|---|---|---|
| USB Type-C Programming/Power | `与同组接口任选其一` | `main_power_input` | `Provide 5 V input; optionally provide firmware download and serial debugging.` |
| XT30 5V Power Input | `与同组接口任选其一` | `main_power_input` | `Provide the nominal 5 V board input when USB is not used as the power source.` |
| UART1 GH1.25-4P | `按项目需要连接` | 留空 | `Connect an external 3.3 V TTL UART device.` |
| CAN GH1.25-4P | `按项目需要连接` | 留空 | `Connect the board to a CAN bus.` |
| I2C0 GH1.25-4P | `按项目需要连接` | 留空 | `Connect external I2C target devices.` |
| 18-Pin Signal Expansion Header | `按项目需要连接` | 留空 | `Connect signal resources to the matching adapter board.` |
| 12-Pin Power Expansion Header | `按项目需要连接` | 留空 | `Supply the matching adapter board when used.` |

#### 通讯端点角色

- I2C：模块支持 `控制端/controller`，当前工作方式固定为该角色；切换方式选择 `固件配置`。
- USB：如果接口标准启用了角色校验，选择 `device/设备端`；实际角色代码必须以接口标准目录为准。
- CAN：应使用“端点等价/无需区分角色”的接口标准，不为主控板发明 CAN 主端或从端。
- UART：如果现有标准定义了对等端点，保持无需区分；如果定义了角色，只选择硬件和固件真实支持的角色。

#### 运行供电条件组

添加一组：

| 字段 | 内容 |
|---|---|
| 供电用途 | `Main board normal-operation 5 V input` |
| 最少接通几路 | `1` |
| 最多允许接通几路 | `1` |
| 可用于满足本组的取电关系 | USB Type-C 的 5 V load 关系、XT30 的 5 V load 关系 |

这样可表达“Type-C 或 XT30 二选一供电”，同时避免自动拓扑把两个 5 V 输入同时接通。

---

## 5. 接口供电关系

建议录入 7 条。所有电流字段暂时留空，不复制旧 Revision 的估算值。

| # | 关系 | 电源轨 | 所属接口 | 电压 | 电流字段 | 验证状态/备注 |
|---:|---|---|---|---|---|---|
| 1 | 从接口取电 | `5V` | USB Type-C | `5.0–5.0 V` | 留空 | 原理图确认；整板电流待实测 |
| 2 | 从接口取电 | `5V` | XT30 5V Power Input | `5.0–5.0 V` | 留空 | 原理图确认；整板电流待实测 |
| 3 | 向外供电 | `3V3` | UART1 GH1.25-4P | `3.3–3.3 V` | 留空 | 最大持续/峰值电流待验证 |
| 4 | 向外供电 | `3V3` | CAN GH1.25-4P | `3.3–3.3 V` | 留空 | 最大持续/峰值电流待验证 |
| 5 | 向外供电 | `3V3` | I2C0 GH1.25-4P | `3.3–3.3 V` | 留空 | 最大持续/峰值电流待验证 |
| 6 | 向外供电 | `3V3` | 12-Pin Power Expansion Header | `3.3–3.3 V` | 留空 | AMS1117 热限制和总负载待验证 |
| 7 | 向外供电 | `5V` | 12-Pin Power Expansion Header | `5.0–5.0 V` | 留空 | 取决于输入源、保险丝、走线和其他负载 |

不要把 18-Pin Signal Header 录为供电接口；其引脚表中没有电源脚。

---

## 6. 其他工程参数

以下条目可直接按“标签 / 值 / criticality / 状态 / 来源”录入。JSON 值建议保持原样。

### 6.0 网页类别和来源绑定

| 工程参数 | category | criticality | 首选依据资料 |
|---|---|---|---|
| chip | `control` | `engineering_critical` | 新版主控原理图 |
| memory | `control` | `engineering_critical` | ESP32-WROOM-32E-N4 制造商文档 |
| wireless | `control` | `engineering_critical` | ESP32-WROOM-32E-N4 制造商文档 |
| programming | `control` | `engineering_critical` | 新版主控原理图 |
| power | `electrical` | `engineering_critical` | 新版主控原理图 |
| communication | `control` | `engineering_critical` | 新版主控原理图 |
| gpioPinMap | `control` | `engineering_critical` | 新版主控原理图 |
| io | `control` | `engineering_critical` | 新版主控原理图 |
| connectors | `mechanical` | `engineering_critical` | 新版主控原理图 |
| canTermination | `electrical` | `engineering_critical` | 主控原理图；转接板仅作兼容性来源 |
| electricalConstraints | `electrical` | `engineering_critical` | 主控原理图 + 制造商文档 |
| softwareSupport | `software` | `advisory` | 当前 ESP32 固件仓库审查记录 |

录入时先保持“待审核”；绑定对应来源后，在审核发布步骤执行人工确认。没有实测记录时不得选择“实测验证”。如果后续要开放量产判断，所有 `engineering_critical` 事实必须升级为“文档验证”或“实测验证”，单纯“人工确认”不够。

### 6.1 `main_controller / chip`

```text
ESP32-WROOM-32E-N4
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：新版主控原理图 + 芯片资料

### 6.2 `main_controller / memory`

```json
{"flashMb":4,"psram":false,"moduleVariant":"N4"}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：ESP32-WROOM-32E-N4 芯片资料

### 6.3 `main_controller / wireless`

```json
{"wifi":true,"bluetooth":true,"frequency":"2.4GHz"}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：ESP32-WROOM-32E-N4 芯片资料

### 6.4 `main_controller / programming`

```json
{"usbConnector":"USB Type-C","usbMode":"USB 2.0 device","usbUartChip":"CH340X","uartPort":"UART0","signals":{"tx":"GPIO1/TXD0","rx":"GPIO3/RXD0","boot":"GPIO0","reset":"EN"},"automaticDownload":true,"usbPowerDelivery":false}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：新版主控原理图

### 6.5 `main_controller / power`

```json
{"nominalInputVoltageV":5.0,"inputConnectors":["USB Type-C","XT30"],"logicVoltageV":3.3,"regulator":"AMS1117-3.3S","sharedInputRail":true,"protectedPowerOringVerified":false,"verifiedCurrentLimits":null}
```

- criticality：`engineering_critical`
- 状态：`部分确认`
- 已确认：输入接口、5 V/3.3 V 电源轨、稳压器型号。
- 待确认：输入范围、保险丝额定值、反灌条件、温升、持续和峰值负载能力。
- 来源：新版主控原理图；电流能力需要 BOM 和实测证据。

### 6.6 `main_controller / communication`

```json
{"can":{"controller":"ESP32 TWAI","transceiver":"SN65HVD230DR","tx":"GPIO5","rx":"GPIO4","connectorCount":1,"commonModeChoke":"ACT1210-510-2P","terminationOhm":120},"uart":{"programming":{"port":"UART0","tx":"GPIO1","rx":"GPIO3"},"user":{"port":"UART1","tx":"GPIO17","rx":"GPIO16","logicVoltageV":3.3}},"i2c":{"port":"I2C0","role":"controller","sda":"GPIO21","scl":"GPIO22","logicVoltageV":3.3},"i2s":{"ws":"GPIO19","sck":"GPIO18","sd":"GPIO2"}}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：新版主控原理图

### 6.7 `main_controller / gpioPinMap`

```json
{"GPIO0":"BOOT/IO0","GPIO1":"TXD0","GPIO2":"I2S_SD","GPIO3":"RXD0","GPIO4":"CAN_RX","GPIO5":"CAN_TX","GPIO12":"BIN1","GPIO13":"BIN2","GPIO14":"AIN2","GPIO15":"IO15","GPIO16":"RXD1","GPIO17":"TXD1","GPIO18":"I2S_SCK","GPIO19":"I2S_WS","GPIO21":"I2C0_SDA","GPIO22":"I2C0_SCL","GPIO23":"LED","GPIO25":"PWMA","GPIO26":"PWMB","GPIO27":"AIN1","GPIO32":"PWM1","GPIO33":"PWM2","GPIO34":"ADC3","GPIO35":"ADC4","GPIO36/SENSOR_VP":"ADC1","GPIO39/SENSOR_VN":"ADC2"}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：新版主控原理图

### 6.8 `main_controller / io`

```json
{"pwm":["PWM1/GPIO32","PWM2/GPIO33","PWMA/GPIO25","PWMB/GPIO26"],"adc":["ADC1/SENSOR_VP/GPIO36","ADC2/SENSOR_VN/GPIO39","ADC3/GPIO34","ADC4/GPIO35"],"motorControl":{"channelA":["AIN1/GPIO27","AIN2/GPIO14","PWMA/GPIO25"],"channelB":["BIN1/GPIO12","BIN2/GPIO13","PWMB/GPIO26"]},"i2s":["I2S_WS/GPIO19","I2S_SCK/GPIO18","I2S_SD/GPIO2"],"gpio":["IO15/GPIO15"],"led":"GPIO23"}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 来源：新版主控原理图

### 6.9 `main_controller / connectors`

```json
{"usbTypeC":1,"xt30PowerInput":1,"uartGh1p25_4p":1,"canGh1p25_4p":1,"i2cGh1p25_4p":1,"signalHeader18Pin":1,"powerHeader12Pin":1,"onboardCanConnectorCount":1,"adapterBoardCanConnectorCount":10}
```

- criticality：`engineering_critical`
- 状态：`人工确认`
- 说明：`adapterBoardCanConnectorCount` 只是兼容性说明，不计入主控模块的统一接口数量。
- 来源：新版主控原理图 + 新版转接板原理图

### 6.10 `main_controller / canTermination`

```json
{"mainBoard":{"resistanceOhm":120,"reference":"R7","populationControlRequired":true},"matchingAdapter":{"resistanceOhm":120,"reference":"R1","populationControlRequired":true},"rule":"Populate termination only at the two physical ends of the CAN bus."}
```

- criticality：`engineering_critical`
- 状态：`原理图确认；装配策略待确认`
- 来源：新版主控原理图 + 新版转接板原理图

### 6.11 `main_controller / electricalConstraints`

```json
{"gpioLogicVoltageV":3.3,"gpio5VTolerant":false,"inputOnlyPins":["GPIO34","GPIO35","GPIO36/SENSOR_VP","GPIO39/SENSOR_VN"],"strappingPins":["GPIO0","GPIO2","GPIO5","GPIO12","GPIO15"],"usbAndXt30Share5VRail":true,"simultaneousPowerInputsApproved":false,"externalLoadCurrentVerified":false}
```

- criticality：`engineering_critical`
- 状态：`部分确认`
- 来源：新版主控原理图 + 芯片资料；同时供电和负载能力待硬件负责人确认。

### 6.12 `main_controller / softwareSupport`

```json
{"platform":"ESP32-WROOM-32E-N4","framework":"ESP-IDF","boardProfile":"boards/my_board_esp32wroom/board.h","implementedReusableApis":{"gpio":["bsp_gpio_config","bsp_gpio_set_level","bsp_gpio_get_level","bsp_gpio_reset"],"i2c":["bsp_i2c_init","bsp_i2c_deinit","bsp_i2c_add_device","bsp_i2c_remove_device","bsp_i2c_probe","bsp_i2c_reset","bsp_i2c_read","bsp_i2c_write"],"uart":["bsp_uart_init","bsp_uart_init_default","bsp_uart_read","bsp_uart_write","bsp_uart_deinit"],"pwm":["bsp_pwm_timer_init","bsp_pwm_channel_init","bsp_pwm_set_duty"]},"directFrameworkUsage":{"can":"ESP-IDF TWAI examples; no shared bsp_can API verified"},"missingReusableCoverage":["ADC","I2S"],"knownDrift":["BOARD_LED_GPIO is GPIO12 in the current board profile but GPIO23 in the V1.0 schematic"],"firmwareGenerationReady":false}
```

- criticality：`advisory`
- 状态：`仓库人工审查`
- 来源：当前 ESP32 固件仓库，不使用旧 Revision 中未找到的虚构 API。

### 6.13 `main_controller / estimatedCostRmb`

本 Revision 暂不录入。必须等新版 BOM 与实际采购单绑定后再填写；旧版 `35–80 RMB` 不可直接复制。

---

## 7. 驱动与软件支持显示内容

如果网页要求单独维护“驱动与软件支持”，建议使用以下对象：

```json
[
  {
    "name": "esp32_wroom32e_board_support",
    "version": "unreleased-v1.0-schematic-alignment",
    "platform": "ESP32-WROOM-32E-N4",
    "framework": "ESP-IDF",
    "protocol": "GPIO/UART/I2C/PWM/TWAI",
    "header": "boards/my_board_esp32wroom/board.h",
    "publicApis": [
      "bsp_gpio_config",
      "bsp_gpio_set_level",
      "bsp_gpio_get_level",
      "bsp_gpio_reset",
      "bsp_i2c_init",
      "bsp_i2c_deinit",
      "bsp_i2c_add_device",
      "bsp_i2c_remove_device",
      "bsp_i2c_probe",
      "bsp_i2c_reset",
      "bsp_i2c_read",
      "bsp_i2c_write",
      "bsp_uart_init",
      "bsp_uart_init_default",
      "bsp_uart_read",
      "bsp_uart_write",
      "bsp_uart_deinit",
      "bsp_pwm_timer_init",
      "bsp_pwm_channel_init",
      "bsp_pwm_set_duty"
    ],
    "dependencies": [
      "ESP-IDF GPIO driver",
      "ESP-IDF UART driver",
      "ESP-IDF I2C driver",
      "ESP-IDF LEDC driver",
      "ESP-IDF TWAI driver"
    ],
    "implementedFeatures": [
      "USB UART programming through CH340X",
      "Reusable GPIO support",
      "Reusable UART support",
      "Reusable I2C support",
      "Reusable PWM support",
      "TWAI usage in examples"
    ],
    "missingOrUnverifiedFeatures": [
      "Shared CAN/TWAI BSP API",
      "Reusable ADC board API",
      "Reusable I2S board API",
      "V1.0 schematic-aligned board profile build verification",
      "Hardware-in-the-loop validation"
    ],
    "knownIssues": [
      "Current BOARD_LED_GPIO is GPIO12 while the V1.0 schematic assigns LED to GPIO23.",
      "GPIO is not 5 V tolerant.",
      "The 3.3 V external-load current limit has not been verified.",
      "CAN termination population must follow the physical bus topology."
    ],
    "license": "Internal",
    "sourceType": "repository review",
    "firmwareGenerationReady": false
  }
]
```

禁止继续填写以下旧 API，除非代码中实际实现并完成构建验证：

- `board_init`
- `board_can_init`
- `board_uart_init`
- `board_i2c_init`
- `board_pwm_init`
- `board_adc_read`
- `board_i2s_init`

---

## 8. 来源与文件资产

### 8.0 按网页“资料来源”步骤新增

#### 来源 1：新版主控原理图

| 字段 | 内容 |
|---|---|
| 事实来源类型 | `电路原理图` |
| 来源标题 | `ESP32 Main Controller V1.0 Schematic 2026-08-14` |
| 人工说明/文档摘录 | `Two-page V1.0 schematic for the ESP32-WROOM-32E-N4 main controller. Page 1 covers the ESP32 module, USB Type-C/CH340X download circuit, 5 V input, AMS1117-3.3S regulator, and SN65HVD230DR CAN physical layer. Page 2 covers the H1/H2 expansion headers, UART1, I2C0, and XT30 connectors.` |
| 上传文件 | `SCH_ESP32-WROOM-32_2026-08-14.pdf` |
| 文件资产类型 | 选择 `原理图/硬件设计` 对应选项 |

#### 来源 2：ESP32-WROOM-32E-N4 制造商文档

| 字段 | 内容 |
|---|---|
| 事实来源类型 | `制造商文档` |
| 来源标题 | `ESP32-WROOM-32E/32E-N4 Manufacturer Documentation` |
| 人工说明/文档摘录 | `Used for module memory, wireless capability, GPIO electrical limits, input-only pins, and boot-strapping pin constraints. Reuse the existing asset only after confirming that its exact device family and document revision apply to ESP32-WROOM-32E-N4.` |
| 上传文件 | 可复用已核对的芯片资料；未核对前不要绑定旧资产 |

#### 来源 3：当前固件仓库审查

| 字段 | 内容 |
|---|---|
| 事实来源类型 | `驱动代码` |
| 来源标题 | `ESP32 Firmware Repository Review 2026-08-17` |
| 人工说明/文档摘录 | `Reusable bsp_gpio, bsp_i2c, bsp_uart, and bsp_pwm APIs were found. TWAI is used directly by examples. No shared ADC or I2S board API was verified. The current board profile assigns BOARD_LED_GPIO to GPIO12 while the V1.0 schematic assigns the LED signal to GPIO23.` |
| 代码适用范围 | `指定主控或平台` |
| 适用主控 | `my_board_esp32wroom@V1.0` |
| 适用平台 | `ESP32` |
| 适用框架 ID | `esp-idf`；若目录中的真实框架 ID 不同，以目录为准 |

新版转接板原理图默认不上传为主控板的主要资产。只有在记录 H1/H2 兼容性或 CAN 终端装配冲突时，才以“兼容性参考”关联；转接板自身接口和能力应建立独立模块 Revision。

### 8.1 新 Revision 应新增或重新绑定

| 资料 | 类型 | 是否可作为关键来源 | 处理建议 |
|---|---|---|---|
| `SCH_ESP32-WROOM-32_2026-08-14.pdf` | 硬件设计/原理图 | 是 | 必须上传并绑定本 Revision |
| ESP32-WROOM-32E/32E-N4 官方资料 | 制造商文档 | 是 | 芯片型号一致时可复用，但需保留版本信息 |
| 当前 ESP32 固件仓库审查记录 | 驱动代码/人工审查 | 是，限软件支持 | 在修正 GPIO23 前标记待审核 |
| `SCH_ESP32转接板新版_2026-08-14.pdf` | 独立模块原理图 | 仅用于兼容性和边界说明 | 应主要绑定到独立的转接板模块，不计为主控本体资产 |

### 8.2 不得未经核对直接复用

- `Gerber_ESP32 Main Controller Board.zip`
- `BOM_ESP32 Main Controller Board.xlsx`
- `driver_esp32wroom.zip`
- `main.c`
- 旧 `SCH_ESP32 Main Controller Board.pdf`

只有文件中的硬件版本、日期、网络表和新版 V1.0 一致时，才能绑定到新 Revision。SHA-256 不同只说明文件不同，不能证明哪个版本正确。

### 8.3 推荐来源记录名称

1. `ESP32 Main Controller V1.0 Schematic 2026-08-14`
2. `ESP32-WROOM-32E-N4 Manufacturer Documentation`
3. `ESP32 Firmware Repository Review 2026-08-17`
4. `ESP32 Adapter V1.0 Schematic 2026-08-14 - compatibility reference only`

---

## 9. 就绪度与发布建议

网页会按 Schema 自动计算就绪度时，不要为了显示“可用”补造数值或 API。

| 环节 | 当前建议 | 原因 | 转为可用的最低证据 |
|---|---|---|---|
| 器件选型 | 可用 | 主芯片、收发器、桥接芯片和稳压器型号已在原理图明确 | 原理图来源成功绑定 |
| 连接拓扑 | 暂不可用/待审核 | 18Pin、12Pin 接口标准和与转接板的装配方向尚未形成受控接口契约 | 建立接口标准并复核 PCB 装配图/网络表 |
| 固件生成 | 不可用 | LED 引脚漂移；CAN/ADC/I2S 公共支持与旧声明不一致 | 修正板级配置、补齐真实 API、编译通过并绑定 commit |
| 量产判断 | 不可用 | 缺少新版 BOM/Gerber 绑定、功耗/温升、EMC、长稳和故障注入证据 | 量产资料与验证报告齐全 |

建议新 Revision 首次保存为“草稿/待审核”，完成下面的最小核对后再发布：

1. 确认 H1/H2 与转接板 H3/H4 的实际对插方向和每个网络的连续性。
2. 确认 R7 及转接板 R1 的装配策略。
3. 修正 `BOARD_LED_GPIO` 为 GPIO23，并对照原理图复核整个 `board.h`。
4. 编译 ESP-IDF 工程，确认 GPIO/UART/I2C/PWM 接口不破坏现有示例。
5. 用万用表/示波器核对 5 V、3.3 V、EN、IO0、UART0 和 CANH/CANL。
6. 通过负载和温升测试后再填写持续/峰值电流。

---

## 10. 已发现的事实冲突

| 项目 | 新版原理图 | 旧内容/当前仓库 | 本草案处理 |
|---|---|---|---|
| LED | GPIO23 | 当前 `board.h` 为 GPIO12 | 以原理图为准，代码标记待修正 |
| I2C SCL | GPIO22 | 当前 `board.h` 为 GPIO22；部分旧参考表曾写 GPIO23 | 以原理图 GPIO22 为准 |
| I2C SDA | GPIO21 | 当前 `board.h` 为 GPIO21 | 保留 |
| 主控 CAN 插座数量 | 1 | 旧上下文曾合并为 10 | 主控填 1；转接板单独填 10 |
| CAN 终端 | 主控 R7=120 Ω | 转接板另有 R1=120 Ω | 写为装配策略待确认 |
| 软件 API | 实际 BSP 为 `bsp_*` | 旧 Revision 填写 `board_*` | 只录实际查到的 API |
| 供电能力 | 未验证 | 旧 Revision 填 650/100/102 mA | 全部留空待实测 |
| 成本 | 新 BOM 未核对 | 旧 Revision 填 35–80 RMB | 删除/暂不填写 |

---

## 11. 审核结论

这份内容足以创建新版主控 Revision 的草稿并完成器件、接口、引脚和板级功能描述，但当前证据不足以把“连接拓扑”“固件生成”“量产判断”全部标为可用。最关键的阻塞是板对板接口契约、GPIO23 代码漂移、CAN 终端装配策略以及未经验证的供电电流。

---

## 12. 稍后实际填写顺序

为了避免后面的能力和接口没有来源可选，按以下顺序操作：

1. 点击旧模块的“编辑新 Revision”，不要编辑或删除 Revision 50。
2. 在“基础说明”填写第 1、2 节；资料成熟度选择“资料核验中”，只允许“原型开发”。
3. 在“资料来源”先添加第 8.0 节的新版主控原理图；核对芯片资料后再添加制造商文档和代码审查来源。
4. 在“工程候选”依次添加第 3 节能力、第 4 节接口和工作方式、第 5 节供电关系、第 6 节工程事实。
5. 为每条能力、接口、供电关系和非 advisory 工程事实绑定至少一个有效来源。
6. 18Pin/12Pin 标准若不存在，先保存草稿，再去接口标准页面建立并启用正确标准；不要临时选一个相似但错误的标准。
7. 回到“审核发布”，逐条核对并点击人工确认，处理页面提出的接口角色和标准建议。
8. 先保存草稿并查看系统生成的阻塞列表；在 GPIO23、板对板接口和 CAN 终端策略处理前，不建议正式发布。

### 发布前必须人工复核的三项

- `H1/H2` 与新版转接板实际对插后的连续性，不能只根据两张原理图符号朝向判断。
- `R7` 与转接板 `R1` 的装配策略，保证整条 CAN 总线只有物理两端各 120 Ω。
- `BOARD_LED_GPIO` 从 GPIO12 到 GPIO23 的代码修正、ESP-IDF 编译结果和最小上板日志。

---

## 13. 具体编辑页面：四阶段字段分类记录

本节按照网页真实页面顺序记录，稍后填写时只需要从上到下执行。网页中的四阶段名称和说明分别为：

| 阶段 | 页面名称 | 页面说明 | 本 Revision 的目标 |
|---:|---|---|---|
| 1 | `基础说明` | `先用自然语言把模块说清楚` | 冻结模块身份和适用边界 |
| 2 | `资料来源` | `上传文件或补充人工说明，并让 AI 提取候选` | 先建立事实来源，再填工程内容 |
| 3 | `工程候选` | `维护能力、接口、供电和关键事实` | 把新版原理图转成结构化能力、接口和约束 |
| 4 | `审核发布` | `确认关键事实与 AI 追问后发布` | 逐条绑定证据、确认、验证拓扑并发布 |

### 阶段 1：基础说明

| 页面字段 | ESP32 新版主控填写内容 | 是否可直接填写 | 备注 |
|---|---|---|---|
| 模块名称 | `ESP32 Main Controller Board` | 是 | 保持既有模块名称 |
| 模块编码 | `MOCE-ESP` | 是 | 稳定代码，不修改 |
| 模块类型 | `主控 PCBA` | 是 | 网页值 `controller_pcba` |
| 集成层级 | `基元` | 是 | 网页值 `primitive`；主控和转接板分开建模 |
| 厂商 | 留空/待公司确认 | 否 | 原理图没有给出模块厂商事实 |
| 硬件版本 | `V1.0` | 是 | 来自新版原理图标题栏 |
| 一句话说明 | 使用第 2.1 节 Human Summary | 是 | 直接复制 |
| 完整描述 | 使用第 2.2 节 Details | 是 | 直接复制 |
| 适用场景 | 使用第 2.3 节 4 条内容 | 是 | 每条单独输入并回车 |
| 限制 | 使用第 2.4 节 11 条内容 | 是 | 每条单独输入并回车 |
| 资料成熟度 | `资料核验中` | 是 | 网页值 `validating` |
| 当前发布状态 | 系统只读 | 不填 | 编辑新 Revision 后为草稿 |
| 允许用途：原型开发 | 勾选 | 是 | 当前允许 |
| 允许用途：量产判断 | 不勾选 | 是 | 缺少量产级证据 |

阶段 1 完成标准：名称、编码、类型、层级、版本、说明、场景、限制和原型用途均已填写；厂商允许暂时留空。

### 阶段 2：资料来源

#### 页面可见字段

| 字段组 | 页面字段 |
|---|---|
| 来源身份 | 事实来源类型、来源标题、人工说明/文档摘录 |
| 文件 | 上传文件（可选）、文件资产类型 |
| 代码文件附加字段 | 代码作用域、适用主控、适用平台、适用框架 ID |
| 页面动作 | 添加来源、AI 提取工程信息、删除来源 |

#### 本 Revision 的来源记录

| 顺序 | 事实来源类型 | 文件资产类型 | 标题 | 文件/内容 | 用途 |
|---:|---|---|---|---|---|
| 1 | `电路原理图` | `电路原理图` | `ESP32 Main Controller V1.0 Schematic 2026-08-14` | 上传新版主控 PDF，并使用第 8.0 节摘录 | 能力、接口、供电、引脚和器件的主来源 |
| 2 | `制造商文档` | `芯片 Datasheet` | `ESP32-WROOM-32E-N4 Manufacturer Documentation` | 复用前先核对型号和文档版本 | 内存、无线、GPIO 电气和启动约束 |
| 3 | `驱动代码` | `驱动代码` | `ESP32 Firmware Repository Review 2026-08-17` | 使用第 8.0 节代码审查说明 | 只证明实际存在的 `bsp_*` API 和当前漂移 |

代码来源附加字段：

| 页面字段 | 内容 |
|---|---|
| 代码作用域 | `指定主控或平台` |
| 适用主控 | `my_board_esp32wroom@V1.0` |
| 适用平台 | `ESP32` |
| 适用框架 ID | `esp-idf`；若系统目录使用其他稳定 ID，以目录为准 |

阶段 2 操作顺序：先添加新版主控原理图，再添加已核对的芯片资料，最后添加代码审查来源。可以使用“AI 提取工程信息”生成候选，但候选不能直接视为已验证事实。

### 阶段 3：工程候选

当前页面由六组内容构成。页面没有提供 `hardwareResources`、`pins` 或 `boardMappings` 的手工编辑入口；相关信息必须写入统一接口标准和工程参数。

#### 3A. 模块自身能力

每项能力的页面字段：

- 能力简称
- 能力类型
- 能力数量
- 完整能力说明
- 实现方式或补充说明
- 能力输入
- 能力输出
- 能力限制
- 依据资料

本 Revision 添加第 3 节的 11 项能力：

1. Embedded Processing and Control
2. 2.4 GHz Wi-Fi and Bluetooth Communication
3. USB Programming and Debugging
4. CAN Bus Communication
5. UART Peripheral Communication
6. I2C Controller Communication
7. I2S Signal Interface
8. Analog Acquisition
9. PWM Output
10. Dual-Channel External Motor-Driver Control
11. Board-to-Board Signal and Power Expansion

所有能力默认关联新版主控原理图；无线和芯片约束同时关联制造商文档；软件实现不能由原理图单独证明。

#### 3B. 接口之间如何协作

页面字段包括：关系名称、关系类型、数据流向、互斥组、所实现能力、A 端接口、B 端接口、补充说明和依据资料。

本 Revision：保持 `0` 条。Type-C/CH340X/UART0、TWAI/SN65HVD230/CAN、5 V/AMS1117/3.3 V 都是板内链路，不应使用两个外部接口伪造接口转换关系。

#### 3C. 工作方式与外部依赖

页面字段包括：

- 条件类型：普通接口、控制、通信、供电、转换/转发、烧录/调试、机械或其他；
- 条件用途；
- 满足条件的接口标准；
- 适用阶段：正常运行、程序烧录、调试、维护或生产测试；
- 连接数量；
- 是否必须满足。

本 Revision 使用默认 `正常工作` 模式，具体接口必要性按第 4.4 节填写。暂不额外添加会强制绑定单一接口标准的工作条件；正常供电的二选一规则使用下一组“运行供电条件”表达。

#### 3D. 运行供电条件

页面字段包括：供电用途、最少接通路数、最多允许接通路数、可满足该条件的取电关系。

本 Revision 添加 1 组：

| 字段 | 内容 |
|---|---|
| 供电用途 | `Main board normal-operation 5 V input` |
| 最少接通路数 | `1` |
| 最多允许接通路数 | `1` |
| 取电关系 | Type-C 5 V load、XT30 5 V load |

#### 3E. 对外接口与供电

每个接口卡片的页面字段：

- 接口标准
- 模块上的接口名称
- 信号方向
- 支持的通讯端点角色
- 当前工作方式中的角色确定方式、固定角色和配置方式
- 当前工作方式的连接要求、连接用途和备选接口组
- 每路随接口供电的使用方式、电压范围、持续/典型电流、峰值电流、启动/浪涌电流
- 依据资料

本 Revision 添加第 4.1 节的 7 个统一接口，并按第 4.4 节设置连接必要性、角色和供电。供电电流全部留空，不能复制旧 Revision 数值。

特别处理：

- Type-C 与 XT30：连接要求都选“与同组接口任选其一”，备选组均为 `main_power_input`。
- I2C：固定为接口标准中的控制端角色。
- CAN：选择端点等价的 CAN 标准，不建立主从角色。
- 18Pin/12Pin：如果目录不存在匹配标准，先创建受控接口标准，再回到模块选择；不要选相似标准凑数。
- 12Pin：标准中需要定义 `3V3` 和 `5V` 两路随接口供电。

#### 3F. 其他工程参数

每条工程参数的页面字段：参数名称、参数值或说明、单位、参数类别、重要程度、资料原文或补充依据、依据资料。

本 Revision 使用第 6 节的 12 条参数和第 6.0 节类别映射。JSON 值必须保持对象结构，不能粘贴成 `[object Object]`。成本字段暂不创建。

阶段 3 完成标准：能力、接口、接口用途、供电二选一关系和关键事实都已建立；每条记录至少关联一个有效来源；不存在伪造接口标准、未说明的接口必要性或未审核的 AI 推断。

### 阶段 4：审核发布

#### 页面审核分组

网页按以下分组列出需要确认的记录：

1. 模块能力
2. 对外接口
3. 工程事实
4. 供电角色
5. 接口关系
6. 硬件资源
7. 资源声明
8. AI 追问与标准建议

本 Revision 正常情况下“接口关系、硬件资源、资源声明”可以为空；其余已创建记录必须有来源并逐条确认。

#### 页面动作及处理规则

| 页面动作 | 本 Revision 的处理 |
|---|---|
| 选择依据资料 | 为缺少来源的记录绑定阶段 2 的原始资料 |
| 人工确认 | 核对内容后逐条点击；仅用于原型判断 |
| AI 追问/标准建议 | 逐条核对；不采纳的建议明确勾选已处理或拒绝 |
| 确认并验证拓扑就绪 | 所有接口标准、接口角色、连接要求和供电组处理后执行 |
| 重新验证拓扑就绪 | 修改任何接口、角色、供电或来源后重新执行 |
| 发布当前版本 | 拓扑验证通过且阻塞项为 0 时才可执行 |

#### 本 Revision 发布前状态记录

| 检查项 | 当前状态 |
|---|---|
| 新版主控原理图已上传并关联 | 待网页填写 |
| 芯片资料版本已核对 | 待确认 |
| 18Pin/12Pin 接口标准存在且启用 | 待网页目录核对 |
| H1/H2 与转接板对插连续性 | 待硬件/PCB 证据 |
| CAN 终端装配策略 | 待确认 |
| `BOARD_LED_GPIO=GPIO23` | 原理图已确认，代码待修正 |
| ESP-IDF 编译 | 未执行 |
| 上板验证 | 未执行 |
| 原型用途发布 | 条件满足后可发布 |
| 量产用途发布 | 当前禁止 |

### 四阶段所有权边界

| 内容 | 应放阶段 | 不应放的位置 |
|---|---|---|
| 板子是什么、能做什么 | 阶段 1 基础说明 | 不用工程参数代替整体说明 |
| 原理图、Datasheet、代码、实测 | 阶段 2 资料来源 | 不把 AI 输出当原始来源 |
| 能力、接口、角色、供电、引脚、限制 | 阶段 3 工程候选 | 不在主控中计入转接板的 10 个 CAN 口 |
| 来源确认、冲突处理、拓扑验证、发布 | 阶段 4 审核发布 | 不把发布成功等同于编译或实机通过 |
