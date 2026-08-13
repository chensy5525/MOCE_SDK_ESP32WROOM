# MSM261DGT003 PDM MEMS 麦克风模块 Card

## Hardware Model
MEMSensing（苏州敏芯微电子）MSM261DGT003，顶部进音、全向、PDM 数字输出 MEMS 麦克风。

- 结论分类：用户资料已确认。
- 证据：模块原理图 `SCH_麦克风_2026-08-03.pdf` 第 1 页 U2；用户提供的数据手册 `FFCE6EA3A1F270F7DA3F96571552317A.pdf` 第 1、2、9 页。

## Module Type
sensor-data（连续音频数据传感器；芯片本身不识别关键词或语音指令）。

- 结论分类：用户资料已确认（PDM 音频输出）+ 基于资料的推断（按本仓库分类规则归入 sensor-data）。

## Communication Interface
PDM 数字音频，ESP32 侧使用 I2S0 的 PDM RX 模式直连；不采用 CH32 桥接。

- 用户称“实际使用接口：I2S”；电气线上实际只有 PDM `CLK` 与 `DATA`，没有标准 I2S 的 `WS/LRCLK`。这里的“I2S”特指 ESP32 I2S 外设的 PDM RX 模式，不应配置为标准 Philips I2S RX。
- 结论分类：用户资料已确认（不桥接、模块引出 SCK/SD）+ 官方资料补充（ESP32 I2S0 支持 PDM-to-PCM RX）+ 基于资料的推断（采用 I2S0 PDM RX）。

## Comm Parameters

### PDM / ESP32 I2S PDM RX
- Signal lines: `SCK/CLK`（ESP32 输出到麦克风）、`SD/DATA`（麦克风输出到 ESP32）。无 WS/LRCLK。
- Microphone raw format: 1-bit, half-cycle PDM；声音瞬时幅度越正，1 的密度越高。
- Clock duty cycle: 40%–60%。
- Sleep mode: `fCLOCK <= 50 kHz`。
- Low-power mode: `150 kHz <= fCLOCK <= 900 kHz`。
- Standard-performance mode: `1.1 MHz <= fCLOCK <= 4.0 MHz`。
- Proposed direct target: 16 kHz, signed 16-bit, mono PCM; PDM-to-PCM down-sampling factor 128, yielding 2.048 MHz PDM clock.
- The 2.048 MHz target is within the microphone's standard-performance range. A 16 kHz / factor-64 setting would yield 1.024 MHz, which lies in the datasheet's undefined 0.9–1.1 MHz gap and must not be the default.
- ESP32 peripheral: I2S0, PDM RX, master clock source, hardware PDM-to-PCM conversion.
- DMA/read block target: 10–20 ms audio per block (160–320 PCM samples at 16 kHz), exact DMA descriptor count `[待确认：第二阶段结合目标 ESP-IDF 版本和可用内存确定]`。
- Evidence: MSM261DGT003 datasheet pp.3, 5, 7; Espressif ESP-IDF I2S PDM RX documentation.
- 结论分类：用户资料已确认（PDM格式、模式频率、占空比）+ 官方资料补充（ESP32换算关系与PDM-to-PCM能力）+ 基于资料的推断（16 kHz、128倍降采样、10–20 ms块）。

### I2C / UART / SPI / GPIO-PWM
- I2C/UART/SPI/standard-I2S: not applicable.
- The L/R pin is a PDM time-slot/edge selector, not an I2S word-select signal.
- 结论分类：用户资料已确认；证据为数据手册 pp.7, 9。

## Pin List
- Module connector pin 1: `3.3V`。
- Module connector pin 2: NC / not populated contact（原理图连接器符号中打叉；不得接入信号）。
- Module connector pin 3: `SCK` -> ESP32 `I2S_SCK`, GPIO18（用户提供的 ESP32 主板原理图第 1 页及转接板原理图第 1 页已确认）。
- Module connector pin 4: `SD` -> ESP32 `I2S_SD`, GPIO2（用户提供的 ESP32 主板原理图第 1 页及转接板原理图第 1 页已确认）。
- Module connector pin 5: `GND`。
- Connector shield/mechanical pins 6 and 7: `GND`。
- On-board slide switch SW2 selects microphone `L/R` between 3.3 V and GND; the selected position determines on which clock half-cycle DATA is driven.
- Implemented board macros: `BOARD_PDM_CLK_GPIO = GPIO18`, `BOARD_PDM_DATA_GPIO = GPIO2`，与主板 `I2S_SCK/I2S_SD` 网络一致；GPIO19 的 `I2S_WS` 不用于该 PDM 麦克风。
- 结论分类：用户资料已确认（连接器、开关、网络名）+ 待硬件验证（SW2实体拨向与高/低位置对应关系、最终GPIO）。证据为模块原理图第1页。

## Hardware Notes
- Module supply: regulated 3.3 V from the connector. The chip itself permits 1.6–3.6 V; absolute maximum supply is 4.0 V.
- Local decoupling: schematic includes C1 = 100 nF across 3.3 V/GND at the microphone. The datasheet recommends 100 nF plus 10 µF near VDD; this module schematic shows no 10 µF local bulk capacitor.
- Power-up time: typical 6 ms, maximum 20 ms after VDD reaches minimum.
- Wake-up time: maximum 200 µs after clock is at least 151 kHz.
- Mode-change time: maximum 10 ms; fall-asleep time maximum 30 µs with clock <=50 kHz.
- Standard-mode typical current: 670 µA at 2.4 MHz, VDD 1.8 V. Actual current at 3.3 V / 2.048 MHz is `[待确认]` and must not be extrapolated.
- Sensitivity: -26 dBFS typical at 94 dB SPL, 1 kHz in standard-performance mode; unit-to-unit stated range -27 to -25 dBFS under datasheet test conditions.
- SNR: 64 dB(A) typical; AOP: 120 dB SPL typical at 10% THD; maximum rated SPL 140 dB SPL.
- Directionality: omnidirectional; nominal frequency-response graph covers approximately 100 Hz–10 kHz, but the datasheet does not specify tolerance limits across that curve.
- Acoustic care: do not obstruct, probe, blow into, vacuum over, wash, ultrasonically clean, or contaminate the top port.
- Known board-level gap: no confirmed ESD components or series termination on the supplied module schematic; cable length/signal integrity limit is `[待确认]`.
- 结论分类：用户资料已确认（原理图和随附芯片手册 pp.3–5, 7, 10, 12）+ 待硬件验证（3.3 V实测电流、线长、噪声与开关位置）。

## Target Observable Behavior
- Direct minimum example: continuously capture audio and print bounded, human-readable results on UART0 without dumping the full 16 kHz PCM stream as text.
- Recommended Stage-2 first phenomenon: every 100 ms print `peak`, `RMS` and a calibrated-or-relative `dBFS` level; additionally print a rate-limited `SOUND_ACTIVE=0/1` result using a configurable threshold. This clearly distinguishes silence, speech/clap and wiring/clock faults while keeping UART bandwidth bounded.
- Optional recognition phenomenon: after the capture path is verified, feed 16 kHz / 16-bit / mono PCM into Espressif ESP-SR AFE + WakeNet + MultiNet and print a semantic command ID/name only on detection. The exact wake word, command list, language/model and serial mapping are `[待用户确认]`.
- The microphone alone cannot determine whether a sound is “拍手”, “开灯” or another instruction; raw PDM/PCM contains only waveform samples. Recognition requires an algorithm/model on ESP32.
- Full PCM-over-UART text output is not recommended: at 16 kHz, even binary PCM is 32 kB/s before framing, while decimal text is much larger and can block or lose real-time samples. If raw capture is needed, use bounded binary chunks or a short WAV capture workflow rather than line-by-line numbers.
- CH32 bridge minimum example: not applicable per user requirement; no bridge package is to be generated.
- 结论分类：用户资料已确认（收音、串口发送、不通过CH32）+ 官方资料补充（ESP-SR输入为16 kHz/16-bit/mono PCM）+ 基于资料的推断（首测打印RMS/peak/dBFS和活动标志）+ 待用户确认（具体指令与串口数据）。

## Identification Method
- No WHO_AM_I register, address, command-response or readable device identifier exists.
- Electrical identification is manual configuration plus runtime plausibility checks: valid PDM clock, DMA receiving continuously, PCM not permanently stuck at one value, and noise/speech energy changes with acoustic stimulus.
- These checks prove a working PDM audio path but cannot uniquely prove the silicon is MSM261DGT003.
- 结论分类：用户资料已确认（仅CLK/DATA、无控制协议）+ 基于资料的推断（运行期合理性检查）+ 待硬件验证。

## Bridge Constraints
- CH32 bridge is explicitly out of scope and not applicable for this module workflow.
- Current knowledge base describes generic CH32 gateways for I2C/UART and mentions SPI classification, but contains no verified CH32 PDM/I2S streaming gateway or required firmware/API.
- PDM audio at 2.048 Mbit/s raw, or 32 kB/s as 16 kHz mono PCM, is a continuous stream and is not compatible with inventing a request/response CAN helper. Any future bridge would require a separately authorized streaming architecture, buffering, clock generation, flow control, framing and loss policy.
- Recommended implementation: direct ESP32 only. Do not create bridge driver/example paths in Stage 3 unless the user opens a new task and supplies/authorizes a verified bridge design.
- 结论分类：用户资料已确认（不桥接）+ 基于资料的推断（带宽与能力缺口）。

## Dependencies
- Depends on: ESP32 I2S0 PDM RX driver/BSP boundary; FreeRTOS/DMA buffering.
- Optional for speech recognition: official Espressif ESP-SR AFE, WakeNet and MultiNet, with a model compatible with the exact ESP32-WROOM flash/PSRAM configuration `[待确认]`.
- Typically used with: none.
- 结论分类：官方资料补充 + 基于资料的推断。

## Software Package Paths
- Direct driver: `components_direct/msm261_direct/`
- Direct example: `examples_direct/msm261_direct_test/`
- Bridge driver: not applicable; do not generate.
- Bridge example: not applicable; do not generate.

## Transport Dependencies
- Direct BSP: current repository has no documented `bsp_i2s`; Stage 2 must either add/extend the target code repository's BSP I2S boundary with authorization or depend directly on the actual ESP-IDF `esp_driver_i2s` component while keeping ESP-IDF types out of the public module API.
- Direct peripheral: `I2S_NUM_0`, PDM RX.
- Bridge protocol layer: not implemented and not required.
- Shared CAN core: not applicable.
- Required CH32 firmware: not applicable.
- 结论分类：基于仓库现状的确认 + 官方资料补充；Stage 2 must re-read the actual target repository APIs before implementation.

## Identification Confidence
- WHO_AM_I available: no.
- Address-only identification: no address/protocol exists.
- Protocol confirmation: none; continuous PDM data plausibility only.
- Manual configuration required: yes.
- Physical chip confidence: high from supplied schematic plus matching supplied datasheet; chip top-mark/assembled-board photo verification `[待确认]`.
- 结论分类：用户资料已确认 + 待硬件验证。

## Authoritative Sources
- Schematic: `C:/Users/chenqing/Desktop/第一批模块资料/麦克风/SCH_麦克风_2026-08-03.pdf`, p.1, V1.0, created/updated 2026-07-29.
- Chip datasheet: `C:/Users/chenqing/Desktop/第一批模块资料/麦克风/FFCE6EA3A1F270F7DA3F96571552317A.pdf`, MSM261DGT003 Data Sheet V1.0, DOC NO. DS-042, Sept. 2021, especially pp.3–7, 9–12.
- ESP32 I2S/PDM official documentation: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
- Official ESP-IDF PDM example/reference: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s/i2s_basic/i2s_pdm
- Official ESP-IDF digital microphone recorder reference: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s/i2s_recorder
- Official ESP-SR command recognition documentation: https://docs.espressif.com/projects/esp-sr/en/latest/esp32/speech_command_recognition/README.html
- CH32 firmware: not applicable.
- Verified ESP32 example: `[待确认]`.
- Hardware validation date/result: `[待确认]`.

## Source Conflicts
- No chip-model, supply or signal-name conflict exists between the supplied schematic and datasheet.
- Terminology clarification, not a source conflict: the module chip outputs PDM, while the requested ESP32-side interface is called I2S. Adopt the authoritative electrical description “PDM microphone received through ESP32 I2S0 PDM RX mode.” Do not treat it as standard I2S with WS.
- The schematic has 100 nF local decoupling; the chip datasheet's recommended interface text asks for 100 nF and 10 µF near VDD. The module-specific schematic is authoritative for what is populated; the chip datasheet is authoritative for the recommended supply design. Hardware testing should determine whether upstream bulk capacitance is sufficient.

## Code Implementation Guide

This guide records Stage-1 implementation research only. It intentionally contains no driver, example, CMake or pseudocode file.

### Operating Model
- **[用户资料已确认] Module behavior:** continuous PDM audio source. The microphone needs power plus an external clock and continuously drives a density-modulated 1-bit stream on its selected half-cycle.
- **[用户资料已确认] Mental model:** sound moves the MEMS diaphragm; the internal fourth-order sigma-delta modulator turns the instantaneous waveform into a fast stream of zeros and ones. More ones means a more positive instantaneous value, not a recognized word or a sound-pressure reading by itself (datasheet pp.2–3).
- **[官方资料补充] Software output model:** ESP32 I2S0 can receive PDM and use hardware filtering/down-sampling to provide signed 16-bit PCM samples. PCM is the waveform consumed by metering, recording or speech-recognition software.
- **[基于资料的推断] Default mode:** use standard-performance mode because 16 kHz PCM with factor-128 PDM-to-PCM conversion produces 2.048 MHz CLK, safely inside 1.1–4.0 MHz.
- **[待硬件验证] Initialization prerequisite:** stable 3.3 V, common ground, confirmed SCK/SD GPIO routing, correct SW2 edge/slot selection and unobstructed acoustic port.

### Initialization Sequence
1. **[用户资料已确认]** Apply stable 3.3 V and keep the acoustic port unobstructed; wait at least the datasheet maximum 20 ms power-up time before judging captured data.
2. **[基于资料的推断]** Allocate a fixed I2S0 RX channel and fixed DMA/buffering resources before starting the microphone clock, so the first continuous samples have somewhere bounded to land and no per-frame allocation is needed.
3. **[官方资料补充]** Configure I2S0 as master PDM RX, PCM output, 16 kHz, 16-bit, mono, down-sampling factor 128, with the confirmed CLK and DIN GPIOs.
4. **[用户资料已确认]** Configure the selected PDM edge/slot consistently with SW2 L/R: L/R=VDD drives DATA after the rising edge and the receiver latches on falling edge; L/R=GND drives after falling and latches on rising (datasheet p.7).
5. **[官方资料补充]** Enable the RX channel/DMA, which starts the continuous clock and capture path.
6. **[用户资料已确认]** Allow up to 10 ms for a mode change and 200 µs wake-up after a valid clock; discard initial transient blocks rather than publishing them.
7. **[基于资料的推断]** Establish readiness only after bounded reads return full blocks and the samples pass plausibility checks (not constant/stuck, finite energy, no DMA error). There is no identity register to read.
- **[基于资料的推断] Order constraint:** power stabilization precedes readiness judgment; DMA is ready before continuous capture; capture starts before sample plausibility is evaluated. Reordering can lose initial data or falsely report a dead microphone.

### Runtime Data or Command Path
```text
acoustic pressure -> 1-bit PDM on SD -> I2S0 DMA/PDM-to-PCM ->
16-bit mono PCM block -> block-length and signal plausibility validation ->
metering or ESP-SR consumer -> rate-limited UART0 result
```
- **[官方资料补充] Capture path:** the receiver reads fixed blocks from DMA with a bounded timeout; the hardware converter performs low-pass filtering/down-sampling and yields 16-bit PCM.
- **[基于资料的推断] Basic metering path:** remove/track DC bias, compute block peak and RMS, convert normalized RMS to `dBFS = 20*log10(rms/32768)` when RMS > 0, and compare a configurable threshold/hysteresis for `SOUND_ACTIVE`. dBFS is relative to digital full scale, not calibrated dB SPL.
- **[官方资料补充] Recognition path:** ESP-SR AFE consumes 16 kHz, 16-bit, mono audio; WakeNet opens the command window and MultiNet returns detection state plus command results. Recognition results are algorithm output, not microphone register data.
- **[基于资料的推断] Blocking points:** DMA read blocks for at most one configured block timeout; UART output and recognizer processing must be decoupled from capture using fixed buffers or a bounded latest-data/ring-buffer policy. Never wait indefinitely or format every PCM sample in the capture path.
- **[待硬件验证] Completion evidence:** continuous full DMA blocks prove transport activity; changing RMS/peak under speech or a clap proves acoustic sensitivity; a recognition ID proves only the selected model recognized the input, not universal speech correctness.

### Data Conversion and Validity
- **[用户资料已确认] Raw format:** 1-bit half-cycle PDM, polarity defined as increasing sound pressure -> increasing density of ones (datasheet p.3). The bitstream has no byte-level measurement register, checksum, timestamp or device status field.
- **[官方资料补充] PCM format:** ESP32 PDM RX PCM mode exposes 16-bit data units; target public sample format is signed 16-bit mono at 16 kHz.
- **[基于资料的推断] Level conversion:** `peak_abs = max(abs(sample))`; `rms = sqrt(sum(sample^2)/N)` using a wide accumulator; `dBFS = 20 log10(rms/32768)`. A zero block is reported as silence/stuck-data rather than negative infinity text. Exact numeric thresholds require measured background noise.
- **[用户资料已确认] Acoustic calibration anchor:** typical sensitivity is -26 dBFS for 94 dB SPL at 1 kHz in standard mode under datasheet conditions. This typical value alone is insufficient for precise sound-level-meter output because unit sensitivity, mounting, frequency response and calibration tone vary.
- **[基于资料的推断] Optional approximate SPL:** only after calibration, `dB_SPL ~= measured_dBFS - calibrated_sensitivity_dBFS_at_94dBSPL + 94`. Store calibration explicitly; do not label uncalibrated dBFS as dB SPL.
- **[基于资料的推断] Validity:** require expected byte count, no driver overflow/timeout, non-stuck samples, plausible DC mean and bounded clipping ratio. Repeated all-zero/all-constant/full-scale blocks indicate clock, pin, edge, power or port problems rather than valid silence.
- **[基于资料的推断] Invalid behavior:** drop only the invalid block, increment counters and retain device lifecycle state until a bounded consecutive-failure threshold is crossed. Never silently clamp a bad block into a valid level.

### Timing and Scheduling Characteristics
- **[用户资料已确认] Hardware timing:** power-up <=20 ms, wake-up <=200 µs, mode change <=10 ms, sleep entry <=30 µs; standard clock 1.1–4.0 MHz; clock duty 40%–60% (datasheet pp.3, 5).
- **[官方资料补充] Continuous rate:** 16 kHz PCM x 2 bytes = 32 kB/s before buffering/metadata; factor 128 yields 2.048 MHz PDM CLK.
- **[基于资料的推断] Suggested processing cadence:** capture continuously in 10–20 ms blocks; aggregate 5–10 blocks and print metering at 10 Hz (every 100 ms). This preserves responsiveness without flooding UART0.
- **[基于资料的推断] Recognition cadence:** process each model-required frame continuously, but print only state changes/detections. Do not schedule recognition as an occasional register poll.
- **[基于资料的推断] Period versus minimum gap:** DMA capture is a fixed continuous stream and audio-frame processing has a regular cadence. UART summaries use a periodic presentation cadence. Recognition/event logs use a minimum repeat/cooldown gap and must not “catch up” with bursts after blockage.
- **[基于资料的推断] Work while waiting:** a blocking I2S read occurs in its own capture task or bounded producer context; other tasks run, and serial formatting/recognition consumes previously captured fixed buffers without holding the I2S resource.

### Lifecycle and Recovery
```text
uninitialized -> powered/stabilizing -> I2S configured -> warming/discarding ->
online/capturing -> degraded -> local reinitializing -> online
```
- **[基于资料的推断] Transient failure:** one DMA timeout/short block is discarded and counted; retry the next complete read, with repeated logs rate-limited.
- **[基于资料的推断] Invalid sample:** clipping, silence or a failed plausibility check affects that block and does not by itself prove the device is offline.
- **[基于资料的推断] Consecutive transport failure:** after a fixed threshold `[待确认：建议3个连续块]`, stop/disable only this I2S channel, clear DMA state, wait the documented stabilization/mode interval, then attempt one local reinitialization sequence with bounded retries and cooldown.
- **[基于资料的推断] Finite recovery:** recommend at most 3 immediate init attempts, then enter offline/degraded state and retry locally at a slow cooldown `[待确认：建议5 s]`; never reboot ESP32 or affect unrelated modules.
- **[基于资料的推断] Unrecoverable configuration failure:** invalid GPIO, unsupported IDF API/mode or I2S resource conflict fails initialization with a specific error and preserves diagnostics.
- **[待硬件验证] Physical disconnect recovery:** verify that removing/reconnecting 3.3 V, SCK or SD produces bounded errors and local recovery rather than continuous false audio.

### Direct Implementation Notes
- **[官方资料补充] BSP/API boundary:** Stage 2 must use the actual target repository and exact ESP-IDF version. The current knowledge base has no I2S BSP entry. Use the modern `esp_driver_i2s` PDM RX API when available, or add a project BSP wrapper only with the target repository's conventions; never invent an existing `bsp_i2s` API.
- **[基于资料的推断] Public API:** expose transport-neutral PCM block metadata and/or level result (`sample_rate`, sample count, peak, RMS/dBFS, clipping, validity, counters); keep ESP-IDF channel handles and driver types private.
- **[基于资料的推断] Instance/buffer model:** one static instance by default, fixed DMA descriptors and a fixed-capacity buffer/ring; no heap allocation in the continuous read or per-frame path. Any init-time allocation must be bounded and released on every failure/deinit path.
- **[基于资料的推断] Tasking:** capture is continuous and timing-sensitive; use a producer task or bounded driver-read loop separate from UART formatting and potentially long recognition inference. Prefer fixed ownership or a bounded ring/latest-block policy with explicit overrun counter.
- **[基于资料的推断] Logging:** UART0 only, tag `[MSM261]`; log init/state transitions and rate-limited faults. Never send debug logs on the audio interface and never print every sample.
- **[待硬件验证] GPIO/resource check:** chosen SCK and SD pins must be physically routed on the current board, avoid reserved CAN/I2C/UART/SPI functions unless intentionally reassigned, and SD should be input-capable while SCK must be output-capable.

### CH32 Bridge Implementation Notes
- **[用户资料已确认] Required generic gateway:** none; user explicitly prohibits CH32 bridging for this module.
- **[基于资料的推断] Existing capability assessment:** insufficient/not applicable. The current repository has no verified PDM/I2S streaming gateway, and generic I2C/UART request-response transports do not satisfy continuous clocked audio.
- **[基于资料的推断] Transfer expansion:** raw PDM is about 2.048 Mbit/s at the selected clock; PCM is 32 kB/s before framing. A CAN bridge would add fragmentation, jitter, buffer and loss semantics and cannot be represented as a simple module register read.
- **[基于资料的推断] Discovery/business boundary:** not applicable in the direct-only design.
- **[用户资料已确认] Stage-3 action:** do not generate a CH32 package or claim a bridge phenomenon.

### Module-Specific Difficulties and Common Mistakes
- **[用户资料已确认] Distinguishing characteristic:** this is a PDM microphone, not a standard-I2S codec and not a speech-recognition module.
- **[基于资料的推断] Mistake:** configuring Philips/standard I2S and expecting WS -> no valid audio. Prevention: use I2S0 PDM RX with CLK and DIN only.
- **[基于资料的推断] Mistake:** selecting 16 kHz with factor 64 -> 1.024 MHz, inside the datasheet's undefined 0.9–1.1 MHz clock gap. Prevention: factor 128 -> 2.048 MHz standard-performance mode.
- **[用户资料已确认] Mistake:** mismatching SW2 L/R edge/slot -> constant, shifted, silent or corrupted samples. Prevention: document physical switch position and verify both positions if the driver slot setting is uncertain (datasheet p.7, schematic p.1).
- **[基于资料的推断] Mistake:** treating `-26 dBFS @ 94 dB SPL` as a universal direct conversion -> inaccurate SPL. Prevention: report dBFS until a known acoustic calibrator and mounting-specific calibration are available.
- **[基于资料的推断] Mistake:** printing every PCM sample as decimal -> UART blockage/DMA overrun. Prevention: continuous bounded capture, 10 Hz summaries, detection-only event output, and overrun counters.
- **[基于资料的推断] Mistake:** applying arbitrary gain before validity/clipping checks -> false activity and recognizer degradation. Prevention: preserve raw PCM, measure DC/peak/clipping, then apply documented AFE/gain.
- **[用户资料已确认] Hardware risk:** obstructing or contaminating the top acoustic port permanently changes response; observe datasheet handling rules.
- **[待硬件验证] Validation focus:** clock frequency/duty, switch edge, PCM polarity/amplitude, noise floor, clipping, acoustic response, long-run DMA overrun, power noise and recovery after disconnect.

### Implementation Complexity
- **[基于资料的推断] Direct capture + level reporting:** medium.
- **[基于资料的推断] Direct offline command recognition:** high, because it adds AFE/model selection, memory/flash constraints, frame scheduling, tuning and false-positive validation beyond the microphone driver.
- **[用户资料已确认] CH32 bridge:** not applicable; if separately requested in the future, complexity would be high and require a new verified streaming gateway architecture.
- **[官方资料补充] Reusable official capabilities:** ESP32 I2S0 PDM-to-PCM RX; official ESP-IDF PDM/recorder examples; optional ESP-SR AFE/WakeNet/MultiNet.
- **[基于资料的推断] Reusable current repository capabilities:** global logging/error/lifecycle conventions only; no documented I2S BSP or PDM module package.
- **[基于资料的推断] Generic gaps:** board I2S pin assignment, repository I2S BSP boundary, exact ESP-IDF API version, selected speech-recognition model/configuration and verified serial output contract.

### Hardware Validation Focus
- **[待硬件验证] Electrical:** 3.3 V at the module under capture, common ground, 2.048 MHz SCK and 40%–60% duty cycle, correct SD voltage levels, no pin conflict.
- **[待硬件验证] Digital:** sustained 16 kHz/16-bit/mono blocks, no DMA overruns, correct SW2/slot setting, sensible DC mean and non-stuck samples.
- **[待硬件验证] Acoustic:** silence noise floor, normal speech level at defined distance, clap/strong-sound clipping, response with port unobstructed, unit-to-unit spread.
- **[待硬件验证] Observable behavior:** 10 Hz UART level reports change predictably with sound; `SOUND_ACTIVE` threshold/hysteresis does not chatter; recognition IDs/names match the user-approved command set if ESP-SR is selected.
- **[待硬件验证] Recovery:** SD/SCK/power disconnect and reconnect, bounded timeouts, local recovery and rate-limited logs.

### Guide Evidence Classification
- **Confirmed from user material:** MSM261DGT003 identity; 3.3 V/SCK/SD/GND connector; 100 nF capacitor; L/R slide switch; PDM format, clock modes, edge timing, sensitivity, SNR, current, startup/mode timing and acoustic handling from the supplied schematic and datasheet.
- **Supplemented from official sources:** ESP32 I2S0 PDM RX/PDM-to-PCM capability and factor-64/factor-128 clock relation; official ESP-IDF examples; ESP-SR 16 kHz/16-bit/mono input and WakeNet/MultiNet command path.
- **Inference:** 16 kHz/factor-128 default, DMA/task/buffer separation, 10 Hz metering, dBFS calculation, bounded recovery, direct complexity and lack of bridge suitability.
- **Pending hardware verification:** final GPIOs, SW2 physical orientation/slot, signal integrity, actual 3.3 V current, PCM noise/gain/polarity, thresholds, acoustic calibration, model resource fit, recognition accuracy and disconnect recovery.

## Module-Specific Error Codes
- `ERR_MSM261_I2S_CONFIG` (-10): I2S0 PDM RX configuration, GPIO or resource conflict.
- `ERR_MSM261_READ_TIMEOUT` (-11): bounded DMA read timeout.
- `ERR_MSM261_SHORT_READ` (-12): received block length does not match the requested complete block.
- `ERR_MSM261_STREAM_OVERRUN` (-13): capture/consumer buffering overflow or dropped audio block.
- `ERR_MSM261_STUCK_DATA` (-14): repeated constant/implausible PCM blocks indicate likely clock/data/wiring failure.
- `ERR_MSM261_NOT_CALIBRATED` (-15): calibrated dB SPL requested without a valid calibration record.

## Changelog
- 2026-08-13: Initial Stage-1 card and code implementation research generated from the supplied module schematic, MSM261DGT003 datasheet, repository rules and official Espressif PDM/ESP-SR documentation; no code generated.
