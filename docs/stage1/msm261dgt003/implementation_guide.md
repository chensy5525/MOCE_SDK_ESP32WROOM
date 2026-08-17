# MSM261DGT003 实现导读

status: `stage1_draft`

本文只说明 Stage 2 的建议实现边界和审查入口，不授权继续生成、提交或声明
驱动通过。

## 1. 建议目录

```text
bsp/
└── bsp_i2s/
    ├── CMakeLists.txt
    ├── README.md
    ├── bsp_i2s.c
    └── include/bsp_i2s.h

components_direct/
└── msm261dgt003_direct_pdm/
    ├── CMakeLists.txt
    ├── README.md
    ├── msm261dgt003_direct_pdm.c
    └── include/msm261dgt003_direct_pdm.h

examples_direct/
└── msm261dgt003_direct_pdm_test/
    ├── CMakeLists.txt
    ├── README.md
    └── main/
        ├── CMakeLists.txt
        └── main.c
```

## 2. 分层所有权

| 层 | 应负责 | 禁止负责 |
|---|---|---|
| `bsp_i2s` | I2S0 channel、PDM RX、DMA、GPIO、有限超时读取、释放 | 麦克风型号限制、音量策略、VAD |
| `msm261dgt003_direct_pdm` | 器件采样率/时钟约束、L/R 选择、BSP 调用封装 | 板卡固定 GPIO、ESP-IDF I2S 寄存器细节、业务逻辑 |
| 最小例程 | 绑定 GPIO18/GPIO2、驱动生命周期、峰值/平均绝对值日志 | 校准 SPL、录音存储、产品状态机 |

调用关系：

```text
minimal example
  -> msm261dgt003_direct_pdm
      -> bsp_i2s
          -> ESP-IDF I2S0 PDM RX channel + DMA
```

## 3. 建议最小接口

板级 BSP：

```c
esp_err_t bsp_i2s_pdm_rx_init(const bsp_i2s_pdm_rx_config_t *config);
esp_err_t bsp_i2s_pdm_rx_start(void);
esp_err_t bsp_i2s_pdm_rx_read(int16_t *samples,
                              size_t sample_capacity,
                              size_t *samples_read,
                              uint32_t timeout_ms);
esp_err_t bsp_i2s_pdm_rx_stop(void);
esp_err_t bsp_i2s_pdm_rx_deinit(void);
```

设备层：

```c
esp_err_t msm261dgt003_direct_pdm_config_default(
    Msm261dgt003DirectPdmConfig *config);
esp_err_t msm261dgt003_direct_pdm_init(
    Msm261dgt003DirectPdm *device,
    const Msm261dgt003DirectPdmConfig *config);
esp_err_t msm261dgt003_direct_pdm_start(Msm261dgt003DirectPdm *device);
esp_err_t msm261dgt003_direct_pdm_read(Msm261dgt003DirectPdm *device,
                                       int16_t *samples,
                                       size_t sample_capacity,
                                       size_t *samples_read,
                                       uint32_t timeout_ms);
esp_err_t msm261dgt003_direct_pdm_stop(Msm261dgt003DirectPdm *device);
esp_err_t msm261dgt003_direct_pdm_deinit(Msm261dgt003DirectPdm *device);
```

接口名沿用仓库 `bsp_<peripheral>` 与 `components_direct/<device>_direct_<bus>`
约定。`bsp_i2s` 当前只实现已选设备需要的 PDM RX，后续标准 I2S TX/RX 必须
另行扩展和验证，不能复用名称暗示未实现能力。

## 4. Stage 2 实现检查

- 所有 ESP-IDF 调用检查 `esp_err_t`。
- 初始化结构体显式清零或完整指定字段。
- `switch` 包含 `default`。
- 容量换算检查 `SIZE_MAX` 溢出。
- 读取超时有明确上限，0 ms 语义明确，不允许 `portMAX_DELAY`。
- 初始化失败时检查并记录通道清理结果。
- 不在 ISR 中调用同步读取或日志。
- 明确单任务/应用互斥锁所有权，不伪装成线程安全。
- GPIO18 的 SPI 冲突留给 resource binding/例程处理，不在设备层抢占。
- GPIO2 启动风险写入 README 和 validation，不声称已验证。

## 5. 构建与验证入口

建议完整构建命令：

```powershell
.\tools\build.ps1 examples_direct/msm261dgt003_direct_pdm_test esp32 my_board_esp32wroom
```

若仓库 `third_party/esp-idf` 子模块仍为空，应使用已初始化的本机 ESP-IDF 6.0.2
环境执行同一 example 的 `idf.py -C <absolute-example-path> build`，并在 validation
中记录实际命令、IDF 版本、退出码、警告数和 ELF/BIN 路径。

上板排查顺序固定为：

1. 连接：U1/U2 五针线序、共地、SW2 位置；
2. 电源与时钟：VDD=3.3 V、GPIO18 PDM CLK；
3. 外设配置：I2S0 PDM RX、GPIO 复用、DMA channel；
4. 中断/DMA：DMA 返回和溢出日志；
5. 软件：状态、容量、超时、声道选择；
6. 时序：连续采集、冷启动、下载模式和长时间稳定性。

## 6. Stage 2 完成定义

只有同时满足以下条件，才可提交为完整 Stage 2：

- `bsp/bsp_i2s`、麦克风 component、direct 最小例程三者齐全；
- 完整 example 链接通过，不只是单个 `.c` 对象编译；
- transport/device/recipe/validation context 与实际路径一致；
- validation 明确区分 compile、bench、board、integrated 状态；
- 未经授权不烧录，不把旧原型的实测结果继承给新文件。
