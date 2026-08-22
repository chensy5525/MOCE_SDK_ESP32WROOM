# 闹钟小车 Wi‑Fi 门户复用指南

本例程提供一个可直接复用的 ESP32 配网与闹钟网页骨架：ESP32 创建 `MOCE-AlarmCar`
SoftAP，网页配置闹钟和上游 Wi‑Fi，取得上游 IP 后由 SNTP 校准系统时间。

## 适用范围

- 目标：经典 ESP32，ESP-IDF 6.0.2；
- 入口：`examples_direct/alarm_car_wifi_portal`；
- 可复用组件：`components_direct/wifi_alarm_portal`；
- 当前调试闹钟数量：1 个（`schedule_id=0`）；
- 上游网络凭据：运行时输入，成功取得 IP 后才写入 NVS；
- 安全状态：`development_only`。HTTP 无认证、无 TLS，SoftAP 使用演示密码，不能直接作为产品发布方案。

## 运行模型

```text
手机/电脑 -> MOCE-AlarmCar SoftAP -> http://192.168.4.1
                         |
                         +-> PUT /api/v1/network/config
                             -> RAM 候选凭据
                             -> STA 认证 + DHCP
                             -> GOT_IP
                             -> NVS commit
                             -> SNTP 立即校时
                             -> 每 6 小时复校
```

清除配置不会关闭 STA 接口，因此清除后仍可扫描附近网络；但 `sta_enabled=false`，不会自动连接。
连接或 NVS 提交失败时，旧运行时配置恢复；没有旧配置时保持未配置状态。

## 网页接口

| 方法 | 路径 | 用途 |
|---|---|---|
| GET | `/api/v1/network/status` | 查看 SSID、连接/配置状态和失败原因，不返回密码 |
| PUT | `/api/v1/network/config` | 提交 `{ "ssid": "...", "password": "..." }`，异步验证 |
| DELETE | `/api/v1/network/config` | 写入未配置 tombstone |
| POST | `/api/v1/network/scan` | 启动一次有界扫描 |
| GET | `/api/v1/network/scan` | 返回最多 10 个 SSID、RSSI 和认证模式 |

密码格式为空或 8–63 个可打印 ASCII 字符；网页和固件都做有界校验。密码不会出现在日志、状态响应或文档示例中。

## NVS 约定

- 命名空间：`alarm_net`；键：`active`；
- 记录含 magic、version、configured flag、SSID、密码和 checksum；
- 成功联网并取得 IP 后 `set_blob + commit`；
- 清除写入显式未配置记录，不使用隐式恢复编译期凭据；
- 读取记录失败、连接失败或写入失败都不能破坏最后一个有效运行时配置。

## 校时行为

`main.c` 始终创建网络校时任务，因此网页动态配网也能触发校时。STA 获得 IP 后，任务进入 SNTP 会话并将结果送入闹钟控制器；成功后等待配置的校准周期，默认 `21600 s`（6 小时）。未配网时任务保持低频等待，不会把手机 HTTP 时间当作权威时间。

## 集成步骤

1. 复制 `components_direct/wifi_alarm_portal` 到目标 ESP-IDF 工程，并在组件依赖中保留 `esp_wifi`、`esp_http_server`、`nvs_flash`、`lwip` 等依赖。
2. 参考 `main/main.c` 创建 `WifiAlarmPortalOptions`，将事件回调接到产品 controller；回调只做有界、非阻塞入队。
3. 将 `alarm_car_on_alarm_triggered()` 和 `alarm_car_on_alarm_stopped()` 替换为产品自己的音频/电机命令投递钩子，不在钩子内阻塞。
4. 在 `menuconfig` 设置 SoftAP 参数、SNTP 服务器、UTC 偏移和校准周期；不要把真实网络密码提交到 Git。
5. 只保留目标板的 resource binding，确认 AP、STA、音频、执行器和其他无线资源没有冲突。

## 构建与烧录

```powershell
. 'E:\Espressif\frameworks\esp-idf-v6.0.2\export.ps1'
idf.py -B 'D:\Desktop\MOCE_Firmware_Workspace\alarm_car_wifi_portal_build' `
       -C 'D:\Desktop\Firmware\ESP\zsan\examples_direct\alarm_car_wifi_portal' build

# 烧录前必须单独确认目标、端口和镜像；示例目标为 COM6。
idf.py -B 'D:\Desktop\MOCE_Firmware_Workspace\alarm_car_wifi_portal_build' `
       -C 'D:\Desktop\Firmware\ESP\zsan\examples_direct\alarm_car_wifi_portal' `
       -p COM6 flash
```

构建输出放在 `D:\Desktop\MOCE_Firmware_Workspace`，不应提交 `build/`、ELF、BIN 或串口日志。

## 最小验收清单

1. 启动日志出现 SoftAP 地址和网页 URL；
2. 清除配置后仍能扫描；
3. 正确网络凭据出现 `station obtained an IP address`，之后出现 `station provisioning succeeded; credentials persisted`；
4. 错误凭据或无 DHCP 时不写 NVS，并恢复旧配置；
5. 成功 GOT_IP 后出现 `TIME_SYNCED: source=sntp`；
6. 断电重启后恢复最后一次成功配置；显式清除后不恢复编译期 SSID；
7. 设置未来 1 分钟闹钟，确认 `ALARM_TRIGGERED`、停止事件和 `repeat=false` 的一次性关闭；
8. 保留固件 SHA-256、板卡/芯片、串口日志和测试日期。

## 已知限制

- 当前没有实现“同一手机同时连接 SoftAP 并提供热点”的自动接力流程；手机通常只能二选一，需第二台设备，或后续增加 BLE/接力配网。
- BLE 配网尚未实现；若加入 BLE，应重新评估经典 ESP32 的应用分区余量和手机端协议。
- HTTP 服务无认证/TLS，SoftAP 密码是演示用途；此版本只能标记 development_only。
- RTC、音频播放、扬声器、TB6612、电机和舵机不属于本例程的可用性证明。
- SNTP 成功只证明网络时间获取成功，不代表 RTC 掉电保持、长期漂移、SPL、VAD/AEC 或音频链路已经验证。

## 变更交接

恢复工作时先读取本文件、`README.md` 以及对应 `docs/context/*`，再现场读取 Git 分支和构建产物哈希；不要仅凭聊天记录重建 NVS、网页或硬件状态。
