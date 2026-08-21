# Alarm car Wi-Fi portal

正式组合例程：ESP32 作为 SoftAP，手机连接后通过本地网页配置并验证一个闹钟。

当前调试版本仅启用 `schedule_id=0`，但配置结构保留 schedule ID，后续可以扩展为多个闹钟。

## 职责边界

本例程负责：

- 接收 Portal 的闹钟配置、手机时间同步和停止当前响铃事件；
- 使用长度为 4 的 FreeRTOS 队列隔离 HTTP 回调和调度任务；
- 每秒按本地星期、时、分检查一次闹钟；
- 保证同一个本地分钟最多触发一次；
- 通过串口输出配置、同步、触发和停止事件；
- `repeat=false` 首次触发后，通过 Portal API 持久化 `enabled=false`；
- 暴露非阻塞弱钩子 `alarm_car_on_alarm_triggered()`。

本例程不负责：

- RTC 驱动或 RTC 掉电计时；
- 电机、蜂鸣器、音频解码或功放驱动；
- 产品级账号、加密传输或每台设备独立凭据配置；
- 保证所有手机操作系统自动弹出 captive portal 页面。

冷启动后必须先由手机同步时间。即使系统时钟残留了看似有效的值，收到本次启动的 `TIME_SYNC` 事件前也不会触发闹钟。

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

在 `Alarm car Wi-Fi portal` 菜单中设置 SoftAP SSID、密码和最大连接数。仓库默认密码只用于开发；正式产品必须替换为每台设备独立或由用户配置的凭据。

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
2. 手机向页面同步当前 Unix 时间和 UTC 偏移；
3. 设置未来 1 分钟、勾选当前星期、启用闹钟；
4. 确认串口出现以下日志：

```text
ALARM_CONFIG_APPLIED
TIME_SYNCED
ALARM_TRIGGERED
ALARM_RINGING_STOPPED
```

5. 对 `repeat=false` 的闹钟触发后刷新页面并重新上电，确认其保持 `enabled=false`；
6. 确认未同步时间、未选当天、禁用状态及重复点击停止时不会错误触发。

`ALARM_TRIGGERED` 只证明调度条件成立并调用了预留钩子，不代表电机、蜂鸣器或音频硬件已经动作。

## 并发约束

Portal 事件回调运行在 HTTP server 上下文中，只校验并零等待入队。状态回调只在有界时间内读取互斥锁保护的快照。调度任务是闹钟运行状态的唯一写入者；调用 `wifi_alarm_portal_set_config()` 前不持有状态锁，因此 Portal 同步回调不会形成重入死锁。
