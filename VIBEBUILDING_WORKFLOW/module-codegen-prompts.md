# Three-Stage Module Code-Generation Prompts

The new-module workflow is exactly:

```
card.md + implementation guide -> direct driver/example -> CH32 bridge driver/example
```

Do not introduce extra per-module summary, manifest or acceptance documents.
Replace angle-bracket placeholders before sending a prompt.
Each stage is a separate user-authorized task.  Do not continue automatically
from one stage to the next. Existing-module compositions use the separate
`composition-codegen-prompt.md` workflow and are not a fourth stage here.

## Stage 1 - Generate card.md and implementation guide

```text
请为<模块名>生成card.md。先不要生成代码。

请按顺序阅读：
1. INDEX.md
2. _global/全部文件
3. _templates/module-card-template.md
4. design-guidelines/board-classification.md
5. design-guidelines/code-generation-rules.md
6. 我提供的模块资料

本阶段已明确授权的知识库写入范围只有modules/<模块目录>/card.md。
不得新增其他知识库文件、目录或中间资料。

已知信息：
- 实际芯片型号：<型号或[待确认]>
- 实际使用接口：<I2C/UART/SPI/GPIO/PWM/ADC>
- 模块供电：<电压或[待确认]>
- 直连最小例程现象：<现象>
- CH32桥接最小例程现象：<现象>

要求：
- 按module-card-template.md生成modules/<模块目录>/card.md；
- card.md不仅填写硬件字段，还必须完整填写“Code Implementation Guide”，包括工作模式、
  初始化顺序、运行期数据/命令路径、数据换算与有效性、时序与阻塞点、生命周期与局部恢复、
  直连实现要点、CH32桥接实现要点、模块特有难点、常见错误、复杂度和验证重点；
- 对用户资料缺失但会影响实现理解的内容进行定向互联网搜索。来源优先级为：用户提供的模块板
  原理图/说明、芯片原厂数据手册、原厂应用笔记、原厂SDK/参考驱动、模块厂商资料、目标仓库
  当前实现、可信第三方资料。第三方库和博客只能作为线索，不得作为寄存器/协议的唯一权威来源；
- 关键实现结论必须注明属于“用户资料已确认”“官方资料补充”“基于资料的推断”或
  “待硬件验证”，并尽可能标注文档章节、页码或官方链接；
- 分析直连和桥接实现所需的通用能力，但本阶段不生成任何驱动、例程或伪代码文件；
- 固件任务优先读取芯片手册、模块规格书和原理图；除非接口、地址、
  供电或芯片型号存在冲突，否则不解析STEP、Gerber等机械生产资料；
- 不知道的字段标[待确认]，不得猜测；
- 资料冲突时列出冲突、来源和建议采用的权威来源；
- 只向我询问会改变通信、初始化、驱动行为或验收现象的阻塞问题；
- 不新增module-facts.md、manifest.md或acceptance-test.md。
- 最终聊天回复必须直接给出一份用户可读的“代码实现导读”，不能只让我打开card.md查看。
  聊天导读至少说明：模块怎样工作、初始化为何按该顺序、运行时怎样得到物理量/完成动作、
  主要时序和阻塞特点、直连与CH32桥接的差异、最大实现难点、复杂度及硬件验证重点；
- 最终回复的最后一个章节只列出会阻塞第二阶段或第三阶段代码生成、必须由我确认的问题。
  不阻塞的问题保留在card.md标[待确认]，不要混入最后的问题列表。
```

## Stage 2 - Generate the direct driver and example

```text
card.md中的关键硬件信息已经确认。请直接生成<模块名>的ESP32直连
驱动和最小例程，不要再设计新的工作流，也不要新增中间资料文档。

知识库在本阶段视为只读；所有英文代码、CMake和README写入用户指定的外部目标仓库。

强制阅读顺序：
1. INDEX.md
2. _global/全部文件
3. modules/<模块目录>/card.md
4. design-guidelines/code-generation-rules.md
5. design-guidelines/sdk-reference.md
6. design-guidelines/reuse-or-new.md
7. design-guidelines/min-example-standard.md
8. 与card.md模块类型对应的design-guidelines文件
9. 对应的header和direct模板
10. 目标仓库当前board.h、对应BSP头文件/实现和CMakeLists.txt

实现语义要求：
- 优先使用card.md中已经确认的“Code Implementation Guide”落实工作模式、初始化顺序、
  运行数据/命令路径、换算与有效性、时序、状态恢复和常见错误规避；
- 如果实现导读与card其他硬件字段、权威资料或目标仓库真实API冲突，停止冲突部分并报告，
  不得自行选择一种含义继续生成；
- 不重新设计模块工作模式，也不把第一阶段标为推断或待硬件验证的内容改写成已确认事实。

根据card.md中的实际物理接口，必须采用以下唯一参考规则：

- 如果是I2C模块，必须阅读并参考：
  components_direct/ssd1315_direct/
  examples_direct/ssd1315_direct_test/

- 如果是UART模块，必须阅读并参考：
  components_direct/syn6288e_direct/
  examples_direct/syn6288e_direct_test/

这里的“参考”只包括：BSP边界、公共头文件边界、目录与CMake组织、
初始化/错误/日志结构、静态实例方式和完整例程组织。禁止复制SSD1315
的显示寄存器、初始化序列、字库、帧缓存和刷新逻辑；禁止复制SYN6288E
的语音帧、GBK编码、播报间隔和语音业务逻辑。当前模块的寄存器、命令、
时序、识别和现象只能来自当前card.md及其权威资料。

实现要求：
- 在components_direct/<模块>_direct/生成本模块独立完整驱动；
- 在examples_direct/<模块>_direct_test/生成完整ESP-IDF例程；
- 新驱动不得依赖SSD1315、SYN6288E或其他具体模块驱动；
- 必须使用目标仓库当前真实BSP API，不直接调用ESP-IDF driver/*；
- 公共模块头文件不暴露ESP-IDF driver/*类型；
- 默认使用静态实例或固定容量实例池；
- 严格遵守命名、日志和错误码规则；
- 最小例程现象：<填写现象>；
- 生成后执行静态边界和结构检查；默认不编译、不烧录、不读取串口，
  由用户进行首次人工编译；仅当本次任务明确要求时才执行这些操作；
- 用户返回编译错误并要求修正后，修正代码并执行增量编译确认；
- 最后报告生成路径、依赖、人工编译命令，并将编译结果和硬件验证明确
  标为“待人工验证”，不要只报告“代码已生成”。
```

## Stage 3 - Generate the CH32 bridge driver and example

```text
直连版本已经完成。请使用同一个card.md直接生成<模块名>的CH32桥接
驱动和最小例程，不要新增中间资料文档。

知识库在本阶段视为只读；所有英文代码、CMake和README写入用户指定的外部目标仓库。

强制阅读顺序：
1. INDEX.md
2. _global/全部文件
3. modules/<模块目录>/card.md
4. design-guidelines/code-generation-rules.md
5. design-guidelines/sdk-reference.md
6. design-guidelines/device-discovery.md
7. design-guidelines/reuse-or-new.md
8. design-guidelines/min-example-standard.md
9. 与card.md模块类型对应的design-guidelines文件
10. 对应的bridge模板

实现语义要求：
- 直连与桥接是独立软件包，但模块工作模式、初始化顺序、数据/命令语义、换算、有效性和
  局部恢复必须共同来自card.md中已经确认的“Code Implementation Guide”；
- 只替换传输实现和桥接超时/分片/稳定节点边界，不得因桥接而改变模块业务协议；
- 如果实现导读要求的通用传输能力在当前网关中不存在，按下方规则报告缺口，不得猜接口。

根据card.md中的下游物理接口，必须采用以下唯一参考链路：

- 如果是CH32-I2C模块，必须阅读并参考：
  components_esp32wroom/ch32_can_gateway_core/
  components_esp32wroom/ch32_i2c_multi_gateway_final/
  components_ch32/ch32_ssd1315_gateway/
  examples_ch32/ssd1315_ch32_test/
  MOCE_SDK_CH32/examples_final/CH32_I2C_gateway_dynamic/main.c

- 如果是CH32-UART模块，必须阅读并参考：
  components_esp32wroom/ch32_can_gateway_core/
  components_esp32wroom/ch32_uart_dynamic_gateway_final/
  components_ch32/ch32_syn6288_gateway/
  examples_ch32/syn6288_ch32_test/
  MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/main.c

这里的“参考”只包括：唯一TWAI所有权、按device_type分流、F0/F1/F2
动态分配、稳定节点生命周期、增量重发现、对应网关公开API、组件依赖、
错误分层日志和桥接例程组织。禁止复制SSD1315的显示寄存器、初始化、
帧缓存和刷新逻辑；禁止复制SYN6288E的语音帧、GBK编码、播报间隔和语音
业务逻辑。当前模块的下游协议和运行逻辑只能来自当前card.md及权威资料。

实现要求：
- 在components_ch32/ch32_<模块>_gateway/生成独立完整桥接驱动；
- 在examples_ch32/<模块>_ch32_test/生成完整ESP-IDF例程；
- 驱动cfg接收发现层拥有的稳定CH32节点引用；
- 驱动不发送F0/F1/F2、不分配ID、不硬编码或长期复制node_id；
- CH32-I2C驱动只调用ch32_i2c_multi_gateway_final；
- CH32-UART驱动只调用ch32_uart_dynamic_gateway_final；
- 模块驱动不得包含或直接调用ch32_can_gateway_core；
- 不得生成can_bridge.h或任何目标仓库中不存在的can_send_xxx接口；
- 例程必须保留动态发现、F2确认、稳定节点表和增量重发现；
- 日志必须分别表示CAN核心、F0/F1/F2发现、下游扫描/通信、模块初始化
  和模块运行失败；
- 最小例程现象：<填写现象>；
- 如果现有通用网关缺少当前模块必需的通用传输能力，先明确报告缺少的
  API及对应CH32固件能力；不得把模块业务逻辑塞进共享网关；
- 生成后执行静态边界和结构检查；默认不编译、不烧录、不读取串口，
  由用户进行首次人工编译；仅当本次任务明确要求时才执行这些操作；
- 用户返回编译错误并要求修正后，修正代码并执行增量编译确认；
- 最后报告生成路径、完整依赖链、人工编译命令，并将编译结果、动态发现
  状态和硬件验证明确标为“待人工验证”。
```

## Reference Boundary Summary

| Target | Mandatory reference | What may be copied as a pattern |
|--------|---------------------|---------------------------------|
| Direct I2C | SSD1315 direct | BSP/header/CMake/example structure |
| Direct UART | SYN6288E direct | BSP/header/CMake/example structure |
| CH32-I2C | OLED bridge chain | Dynamic ID/stable node/I2C gateway structure |
| CH32-UART | SYN bridge chain | Dynamic ID/stable node/UART gateway structure |

No new module copies the reference module's proprietary operating logic.
