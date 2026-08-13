# Existing-Module Composition Code-Generation Prompt

This prompt is exclusively for combining existing modules.  It is not a fourth
stage of the new-module workflow and must not create cards or silently generate
missing single-module packages. Replace angle-bracket placeholders before use.

## Prompt

```text
请使用知识库中已经入库并具有现成驱动的模块，生成一个多模块组合例程。
本任务是“已有模块组合工作流”，不是新模块入库工作流。禁止生成或修改card.md，
禁止自动进入任何模块的直连驱动或CH32桥接驱动生成阶段。
知识库在本任务中完全只读；不得新增或修改任何知识库文件。所有英文代码、CMake、README、
日志和临时产物只能写入用户指定的外部目标仓库。

涉及模块及角色：
- <模块目录1>：<组合中的中性角色，例如measurement_source>
- <模块目录2>：<角色，例如display_consumer>
- <模块目录3>：<角色，例如speech_output>

目标连接方式：<ESP32直连 / CH32桥接 / 明确指定的混合连接>
目标组合现象：<完整描述>
例程输出目录：<目标目录>
允许新增驱动目录：无。现有驱动必须能够原样复用。

强制阅读顺序：
1. INDEX.md
2. _global/全部文件
3. 所有涉及模块的modules/<模块目录>/card.md
4. design-guidelines/code-generation-rules.md，尤其Rule 17
5. design-guidelines/sdk-reference.md
6. design-guidelines/device-discovery.md
7. design-guidelines/reuse-or-new.md
8. design-guidelines/min-example-standard.md，尤其组合例程标准
9. 与所有模块类型对应的design-guidelines文件
10. 每个模块在目标连接方式下的现有公共头文件、CMakeLists.txt和已验证单模块例程
11. 目标连接方式对应的共享BSP或CH32通用网关公共头文件、实现和CMakeLists.txt
12. 目标仓库当前board.h、BSP和顶层CMake组织

工作流边界：
- 先逐模块判断现有驱动是否能在不修改源码、公共API、cfg、寄存器、协议、初始化和行为
  的前提下原样复用；
- 如果任何模块缺少目标连接方式的驱动，或必须修改具体模块驱动/通用网关才能组合，
  立即报告模块、缺少的API/能力、影响和建议进入的独立单模块阶段；不要在本任务中
  偷偷生成、复制、包装或修改驱动，也不要把替代协议塞进组合例程；
- 不重新研究或改写已经确认的模块协议。正常情况下card.md是模块事实入口；只有card、
  驱动和权威资料发生冲突时才报告冲突并停止相关集成路径；
- 只生成本次要求的组合例程，不生成单模块例程或新的中间资料文档。

架构要求：
- 多模块例程默认使用FreeRTOS；AI根据任务周期、阻塞行为、共享资源、最小动作间隔、
  模块数量、生产者/消费者关系和局部恢复边界自行决定任务划分；
- 除非card、用户需求或明确硬实时约束另有规定，所有应用任务使用相同优先级；不得根据
  模块用途或可见程度推断重要性；
- 模块驱动、发现层和组合业务相互解耦。应用层使用有界的最新状态快照、深度1队列、
  事件位或等价机制连接生产者和消费者，不积压过期显示/输出状态；
- 共享锁只覆盖必要的单次传输或最小状态更新，不得在锁内等待业务周期、格式化文本、
  更新无关状态或执行整轮长时间发现加全部初始化；
- 单模块失败只影响该模块角色，并做局部恢复；不得清空稳定节点表、重启正常驱动、
  重启共享CAN核心或重启ESP32；重复错误日志必须限速；
- 固定采样周期可以使用vTaskDelayUntil；“每N秒最多动作一次”必须从上次动作完成/成功
  时刻计算最小间隔，不得在阻塞后追赶并连续补发动作；
- 输出设备采用满足真实链路预算的最高稳定有效速率。OLED必须变化检测、dirty page/region、
  固定宽度覆盖和只保留最新帧；不得复制单模块参考例程中的低速演示延时。

CH32桥接组合的额外要求：
- 保留唯一TWAI所有权、按device_type分流、F0/F1/F2、稳定节点引用和增量合并；
- 启动立即发现；任一声明的目标角色缺失/离线时每10秒增量重发现；全部目标角色在线时
  每30秒增量重发现。完整性按声明角色判断，不能只按节点或地址数量判断；
- 正常模块在重发现时不得重复初始化。CH32-I2C模块初始化以实时probe为当前存在性权威，
  不能因地址不在旧i2c_addrs快照中拒绝已经probe成功的设备；
- 模块驱动只调用对应通用网关，不直接包含或调用ch32_can_gateway_core，不缓存长期node_id。

生成前在内部完成运行预算，不新增预算文档：任务分组、各周期、阻塞点、共享锁、单次批量
输出字节/分片/往返、最坏超时重试、过期状态处理和局部恢复路径。优化顺序为删除重复传输、
只传变化、合并更新、缩小锁范围、解耦阻塞、丢弃过期状态，然后在链路允许范围内提高刷新率。

生成与验证：
- 在<例程输出目录>生成完整ESP-IDF组合例程及README；
- README说明模块角色、完整依赖链、任务划分的机械依据、统一优先级、周期、10秒/30秒发现
  策略、共享资源边界、局部恢复和硬件验收现象，不使用“重要/次要模块”措辞；
- 启动日志打印reset reason和配置的发现/输出周期，并区分boot、rediscovery、rebind、
  module offline/recovery；
- 生成后执行静态边界和结构检查；默认不编译、不烧录、不读取串口，由我进行首次人工编译；
- 如果我返回编译错误并要求修正，修正后执行增量编译确认；
- 最后报告生成路径、原样复用的驱动、完整依赖链、人工编译命令，以及编译、动态发现和
  硬件现象的待验证状态。
```

## Boundary Summary

- New module research belongs to Stage 1 of `module-codegen-prompts.md`.
- Missing direct/bridge capability belongs to a separately authorized
  single-module generation or maintenance task.
- This composition prompt resumes only after those independent packages are
  available and confirmed.
