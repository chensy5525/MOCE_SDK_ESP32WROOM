# Moce SDK

基于 ESP-IDF 的 ESP32-WROOM 外设驱动 SDK。提供统一的外设驱动组件（直连 + CH32 CAN 桥接两种接入方式），以及配套的编译、烧录、串口监视脚本。

---

## 目录结构

```text
.
├── boards/                      # 板级定义
│   └── my_board_esp32wroom/     #   board.h（引脚映射）+ sdkconfig.defaults
├── bsp/                         # 底层抽象（GPIO / I2C / PWM / UART / board）
│   ├── bsp_board/
│   ├── bsp_gpio/
│   ├── bsp_i2c/
│   ├── bsp_pwm/
│   └── bsp_uart/
├── components_direct/           # 直连外设驱动（模块直连 ESP32 总线）
│   ├── mpu6050_direct/
│   ├── msm261_direct/
│   ├── ssd1315_direct/
│   ├── syn6288e_direct/
│   └── vl53l0x_direct/
├── components_ch32/             # CH32 网关驱动（模块挂在 CH32 下游，经 CAN 接入）
│   ├── ch32_mpu6050_gateway/
│   ├── ch32_ssd1315_gateway/
│   ├── ch32_syn6288_gateway/
│   └── ch32_vl53l0x_gateway/
├── components_esp32wroom/       # ESP32 侧 CAN 网关核心组件
│   ├── ch32_can_gateway_core/           # CAN 收发、发现协议核心
│   ├── ch32_i2c_multi_gateway_final/    # I2C 多节点网关
│   └── ch32_uart_dynamic_gateway_final/ # UART 动态网关
├── examples_direct/             # 直连例程
│   ├── mpu6050_direct_test/
│   ├── msm261_direct_test/
│   ├── ssd1315_direct_test/
│   ├── syn6288e_direct_test/
│   └── vl53l0x_direct_test/
├── example_ch32/                # CH32 桥接例程
│   ├── mpu6050_ch32_test/
│   ├── ssd1315_ch32_test/
│   ├── syn6288_ch32_test/
│   └── vl53l0x_ch32_test/
├── env/                         # 环境脚本（install.sh / export.sh）
├── tools/                       # 编译烧录脚本（build / flash / monitor / clean）
├── third_party/
│   └── esp-idf/                 # ESP-IDF submodule
├── .gitmodules
├── .gitignore
└── README.md
```

---

## 环境准备

### 1. 获取 ESP-IDF submodule

```bash
git clone --recurse-submodules <repo-url>
cd moce_sdk
```

如果 clone 时没有拉取 submodule，补拉：

```bash
git submodule update --init --recursive
```

### 2. 安装 ESP-IDF 工具链

Linux / macOS：

```bash
./env/install.sh
```

该脚本会执行 `git submodule update --init --recursive` 并在 `third_party/esp-idf` 下运行 `./install.sh`。

### 3. 激活环境

Linux / macOS：

```bash
source ./env/export.sh
```

Windows PowerShell：

```powershell
. .\third_party\esp-idf\export.ps1
```

---

## 编译与烧录

所有脚本均以 SDK 根目录为基准定位工程，`ProjectDir` 传相对路径（如 `examples_direct/vl53l0x_direct_test`）。

### 编译

Windows PowerShell：

```powershell
.\tools\build.ps1 examples_direct/vl53l0x_direct_test esp32 my_board_esp32wroom
```

Linux / macOS：

```bash
./tools/build.sh examples_direct/vl53l0x_direct_test esp32 my_board_esp32wroom
```

参数说明：
- `ProjectDir`：例程目录（必填）
- `Target`：芯片目标，默认 `esp32`
- `Board`：板级名称，默认 `my_board_esp32wroom`

### 烧录

Windows PowerShell：

```powershell
.\tools\flash.ps1 examples_direct/vl53l0x_direct_test --target esp32 --board my_board_esp32wroom --port COM3
```

Linux / macOS：

```bash
./tools/flash.sh examples_direct/vl53l0x_direct_test --target esp32 --board my_board_esp32wroom --port /dev/ttyUSB0
```

默认串口为 `COM3`，可用 `--port`（或 `-p`）覆盖。

### 串口监视

Windows PowerShell：

```powershell
.\tools\monitor.ps1 examples_direct/vl53l0x_direct_test -Port COM3
```

Linux / macOS：

```bash
./tools/monitor.sh examples_direct/vl53l0x_direct_test COM3
```

### 清理

```bash
./tools/clean.sh examples_direct/vl53l0x_direct_test
```

---

## 板级信息

默认板级 `my_board_esp32wroom` 对应 ESP32-WROOM-32E-N4，引脚映射定义在 `boards/my_board_esp32wroom/board.h`：

| 外设 | 引脚 |
|------|------|
| 编程 / 日志串口 | TXD0 / RXD0 |
| 外部串口（UART1） | TX GPIO17 / RX GPIO16，默认 9600 |
| I2C0 | SDA GPIO21 / SCL GPIO22，400kHz |
| SPI | MOSI 23 / MISO 19 / SCK 18 / CS0 15 / CS1 13 |
| CAN 收发器 | TX GPIO5 / RX GPIO4 |
| 板载 LED | GPIO12 |
| Boot 按键 | GPIO0 |
| PWM 输出（B1~B4） | 32 / 33 / 25 / 26 |
| PWM 输入专用（A1~A4） | 36 / 39 / 34 / 35（ESP32 输入专用，不可做输出） |
| 舵机 | GPIO32 / GPIO33 |
| 电机（TB6612 外部接线映射） | 左 PWM 32 / IN1 33 / IN2 25；右 PWM 26 / IN1 27 / IN2 14 |

> 注意：PWM A1~A4（GPIO36/39/34/35）为 ESP32 输入专用引脚，只能做输入（如编码器），不能驱动输出。

---

## 外设接入方式

每个外设模块支持两种接入方式，对应 `components_direct/` 和 `components_ch32/` 两套驱动：

- **直连（direct）**：模块直接挂在 ESP32 的 I2C / UART / SPI 总线上。驱动调用 `bsp_i2c` / `bsp_uart` 等底层接口直接读写。
- **CH32 桥接（ch32）**：模块挂在 CH32V203 网关下游，CH32 通过 CAN 总线与 ESP32 通信。驱动通过 `ch32_can_gateway_core` 收发 CAN 帧，CH32 负责 CAN ↔ I2C/UART/SPI 转换。

两种接入方式的驱动接口保持一致，上层业务代码不区分模块是直连还是桥接。

---

## 支持的模块

| 模块 | 类型 | 接口 | 直连驱动 | CH32 桥接驱动 |
|------|------|------|----------|---------------|
| VL53L0X | 激光测距传感器 | I2C | `vl53l0x_direct` | `ch32_vl53l0x_gateway` |
| MPU6050 | 六轴姿态传感器 | I2C | `mpu6050_direct` | `ch32_mpu6050_gateway` |
| SSD1315 | OLED 显示屏 | I2C | `ssd1315_direct` | `ch32_ssd1315_gateway` |
| SYN6288E | 语音合成模块 | UART | `syn6288e_direct` | `ch32_syn6288_gateway` |
| MSM261 | 麦克风采集模块 | I2S | `msm261_direct` | — |

---

## 常见问题

### 串口权限（Linux）

烧录 / 监视需要串口权限，把当前用户加入 dialout 组：

```bash
sudo usermod -aG dialout $USER
newgrp dialout
```

### build 目录失效

如果例程目录下存在无效的 build 目录（缺少 CMakeCache.txt 或 build.ninja），`build.ps1` 会自动把它改名为 `build.invalid.<时间戳>` 并重新构建，不会静默失败。
