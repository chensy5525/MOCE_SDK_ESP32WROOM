# MOCE SDK ESP32-WROOM

本工作区只保留 `zsan` 分支正在维护的 ESP32-WROOM 固件、板级支持、
最小例程、context 和构建工具。旧 Claude/Agent 工作流、按设备拆分的
CH32 gateway 代码及无关历史例程已移除。

## 目录结构

```text
boards/             ESP32-WROOM 板级配置与 sdkconfig defaults
bsp/                GPIO、PWM、UART、I2S 等板级封装
components_direct/  当前维护的 ESP32 直连组件
examples_direct/    与组件对应的最小验证例程
context/            保留的历史模块/板级 context
docs/context/       device、transport、recipe、validation 契约
docs/stage1/        已完成的输入冻结与实现导读
docs/moce/          MOCE 草案与工作资料
env/                开发环境脚本
third_party/        ESP-IDF 固定版本 submodule
tools/              构建、清理、烧录和串口监视工具
```

## 当前维护能力

- E104-BT01 UART
- MSM261DGT003 PDM
- MG90S Servo
- TB6612FNG Motor Driver
- Wi-Fi Alarm Portal（当前工作区未提交）

每项能力应包含组件、最小例程，以及适用的 device、transport、recipe、
validation context。历史 context 中若仍引用已移除的旧例程，只能作为历史
线索，不能作为当前可编译或硬件验证通过的证据。

## 初始化 ESP-IDF

首次克隆后初始化固定版本 submodule：

```powershell
git submodule update --init --recursive
```

## 构建

默认构建 TB6612 最小例程：

```powershell
.\tools\build.ps1
```

显式构建其他例程：

```powershell
.\tools\build.ps1 .\examples_direct\servo_direct_test esp32 my_board_esp32wroom
.\tools\build.ps1 .\examples_direct\msm261dgt003_direct_pdm_test esp32 my_board_esp32wroom
.\tools\build.ps1 .\examples_direct\e104_bt01_direct_uart_test esp32 my_board_esp32wroom
```

烧录和串口监视会操作硬件，执行前应确认目标、端口和当前固件：

```powershell
.\tools\flash.ps1 .\examples_direct\tb6612_fixed_duty_test --target esp32 --board my_board_esp32wroom --port COM3
.\tools\monitor.ps1 .\examples_direct\tb6612_fixed_duty_test COM3
```
