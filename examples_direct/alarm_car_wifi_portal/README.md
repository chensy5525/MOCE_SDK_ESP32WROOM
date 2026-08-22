# Alarm car Wi-Fi portal

正式组合例程：ESP32 保留 SoftAP 本地配置页，并可选连接上游 Wi-Fi，使用 SNTP 自动校准系统时间。

当前调试版本仅启用 `schedule_id=0`，但配置结构保留 schedule ID，后续可以扩展为多个闹钟。

## 职责边界

本例程负责：

- 接收 Portal 的闹钟配置和停止当前响铃事件；
- STA 配置存在时，通过短时 SNTP 会话定期获取 UTC 时间；不接受手机 HTTP 时间覆盖；
- 使用长度为 4 的 FreeRTOS 队列隔离 HTTP 回调和调度任务；
- 每秒按本地星期、时、分检查一次闹钟；
- 保证同一个本地分钟最多触发一次；
- 通过串口输出配置、同步、触发和停止事件；
- `repeat=false` 首次触发后，通过 Portal API 持久化 `enabled=false`；
- 暴露非阻塞弱钩子 `alarm_car_on_alarm_triggered()`。
- 暴露 `alarm_car_request_stop()` 给未来 IMU 摇晃检测复用同一高优先级停止通知，并提供 `alarm_car_on_alarm_stopped()` 收尾钩子。

本例程不负责：

- RTC 驱动或 RTC 掉电计时；
- 电机、蜂鸣器、音频解码或功放驱动；
- 产品级账号、加密传输或每台设备独立凭据配置；
- 保证所有手机操作系统自动弹出 captive portal 页面。

冷启动后必须先收到本次启动的 SNTP 样本。即使系统时钟残留了看似有效的值，控制器验收本次 SNTP 样本前也不会触发闹钟。STA 未配置或 SNTP 不可达时，网页仍可修改计划，但闹钟不会按未校准时间触发。

## 星期和重复语义

`weekday_mask` 定义如下：

| 位 | 星期 |
|---|---|
| bit0 | 周一 |
| bit1 | 周二 |
| bit2 | 周三 |
| bit3 | 周四 |
| bit4 | 周五 |
| bit5 | 周六 |
| bit6 | 周日 |

- `repeat=true`：在每个选中的星期重复触发。
- `repeat=false`：第一次触发后持久化关闭该闹钟。
- 网页的“停止当前响铃”只清除本次 `ringing` 状态，不改变未来计划。

## 配置

```powershell
idf.py -C D:\Desktop\Firmware\ESP\zsan\examples_direct\alarm_car_wifi_portal menuconfig
```

在 `Alarm car Wi-Fi portal` 菜单中设置：

- SoftAP SSID、密码和最大连接数；
- 可选的上游 STA SSID/密码；STA SSID 为空时仍保留 APSTA 扫描能力，但不自动连接；
- SNTP 服务器、首次同步超时、重试间隔和本地 UTC 偏移分钟数。
- 成功校准周期；默认 21600 秒（6 小时），可在 60 秒到 24 小时之间配置。

连接设备热点后，也可以在网页“上游 Wi-Fi”区域扫描附近网络或手动填写 SSID/密码。
网页配置优先级高于编译期配置。固件先在 RAM 中试连，取得 IP 后才提交到 NVS；连接
失败或 NVS 提交失败会恢复原网络。点击“清除配置”会持久化未配置状态，重启后不会
重新采用编译期 SSID。状态接口只返回 SSID、连接状态和失败原因，不返回密码。

仓库不保存真实上游网络凭据，运行日志也不会输出密码。默认 SoftAP 密码仅用于开发；正式产品必须改成每台设备独立或由用户配置的凭据。当前 SNTP 提供时间完整性，不提供来源认证；外部网络仍按不可信输入处理。

## 校时数据流

```text
上游 Wi-Fi -> STA 获得 IP -> SNTP 获取 UTC -> 零等待命令队列 -> 闹钟控制器验收
```

网络等待和 SNTP 超时运行在优先级 3 的独立任务中；闹钟调度任务优先级为 5。HTTP 回调只做有界校验和零等待入队，因此联网失败或 DNS 超时不会阻塞闹钟调度，也不会占用未来的音频/电机控制回调。

网络校时任务不依赖编译期 SSID：未配置时低频等待，网页动态配网并取得 IP 后在下一次检查中立即启动 SNTP，通常不超过 1 秒；成功后按默认 6 小时周期复校。

闹钟触发时，控制器先调用音频/运动钩子，再把 Wi-Fi 从 APSTA 切到 AP-only；网页和 captive portal 始终保留。网页停止或未来 IMU 调用 `alarm_car_request_stop()` 后，控制器恢复 APSTA。SNTP 每次成功后立即反初始化，直到下一个校准周期，不常驻占用协议会话。

## 闹钟运行接口

- `alarm_car_on_alarm_triggered(schedule_id)`：启动持续音频和随机运动；只能投递命令，不得阻塞。
- `alarm_car_request_stop()`：网页以外的停止入口，供 IMU 任务完成大加速度/摇晃判定后调用；使用独立原子停止标志和任务通知，不受普通命令队列满影响；不可从 ISR 直接调用。
- `alarm_car_on_alarm_stopped(schedule_id)`：停止音频和运动；只能投递命令，不得阻塞。

IMU 阈值、滤波、方向判定和电机随机运动配方不属于本例程，当前均未实现、未验证。

## 构建

```powershell
$env:IDF_TOOLS_PATH='E:\Espressif'
. 'E:\Espressif\frameworks\esp-idf-v6.0.2\export.ps1'
idf.py --no-ccache -C 'D:\Desktop\Firmware\ESP\zsan\examples_direct\alarm_car_wifi_portal' -B 'D:\Desktop\MOCE_Firmware_Workspace\alarm_car_wifi_portal_build' build
```

审查构建目录放在 `D:\Desktop\MOCE_Firmware_Workspace`，不写入正式 Git 工作区。

## 运行验证

烧录需要单独授权。烧录后按顺序验证：

1. 连接配置的 ESP32 SoftAP，确认手机自动弹出门户页；若系统未弹出，可手动打开 `http://192.168.4.1`；
2. 启用 STA 时确认串口先出现 `station obtained an IP address`，随后出现 `TIME_SYNCED: source=sntp`；未启用或联网失败时，页面应保持“等待网络校时”，且不会由手机时间覆盖；
3. 在网页扫描或手动填写一个测试网络：正确凭据应先连接再保存；错误密码应报告失败并恢复原网络；重新上电后应恢复最后一次成功配置；
4. 设置未来 1 分钟、勾选当前星期、启用闹钟；
5. 确认串口出现以下日志：

```text
ALARM_CONFIG_APPLIED
TIME_SYNCED
ALARM_TRIGGERED
ALARM_RINGING_STOPPED
```

6. 对 `repeat=false` 的闹钟触发后刷新页面并重新上电，确认其保持 `enabled=false`；
7. 确认未同步时间、未选当天、禁用状态及重复点击停止时不会错误触发；
8. 清除上游网络并重新上电，确认仍为未配置状态；模拟 NVS 提交失败时应恢复原网络。

还需分别验证断网冷启动、错误密码、DNS/SNTP 不可达和联网恢复；所有重试均有次数或等待上限。构建通过不能替代这些实网测试。

串口会在 Portal 启动、切为 AP-only、恢复 APSTA、SNTP 释放前后输出 `HEAP_STATUS`，并输出 `NETWORK_TIME_STACK`。合并音频和电机后，应以这些实机最小值而不是单独示例的 map 判断余量。

`ALARM_TRIGGERED` 只证明调度条件成立并调用了预留钩子，不代表电机、蜂鸣器或音频硬件已经动作。

## 并发约束

Portal 事件回调运行在 HTTP server 上下文中，只校验并零等待入队。状态回调只在有界时间内读取互斥锁保护的快照。调度任务是闹钟运行状态的唯一写入者；调用 `wifi_alarm_portal_set_config()` 前不持有状态锁，因此 Portal 同步回调不会形成重入死锁。
