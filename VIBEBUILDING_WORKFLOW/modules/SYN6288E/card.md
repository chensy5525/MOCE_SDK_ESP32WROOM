# SYN6288E Text-to-Speech Module Card

## Hardware Model
Beijing Yutone SYN6288E Chinese text-to-speech chip, LQFP32L, on the supplied 3.3 V UART-to-speech module

## Module Type
hmi-output

## Communication Interface
UART, direct / bridged

## Comm Parameters

### UART
- Factory default: 9600 bps, 8N1, no parity
- Project target: 9600 bps
- Supported chip baud rates: 9600, 19200, and 38400 bps
- Flow control: none
- Interface: full-duplex asynchronous UART
- Maximum complete command frame: 206 bytes
- Maximum data area: 203 bytes
- Maximum text payload per synthesis command: 200 bytes
- Command frame: `0xFD` + 2-byte big-endian data-area length + command/data + XOR checksum
- Module-board polarity: use ordinary non-inverted MCU UART. The SYN6288E chip requires inverted receive data, but the supplied module schematic already implements the MCU-to-chip RX inversion with Q1 (SS8050). Do not invert the UART again in software or in the CH32 gateway.

## Pin List
- Module U1 pin 1: `3V3`
- Module U1 pin 2: `RX`; connect ESP32 `PIN_SYN6288E_TX = GPIO_17` in direct mode, or the CH32 UART TX signal in bridge mode
- Module U1 pin 3: `TX`; connect ESP32 `PIN_SYN6288E_RX = GPIO_16` in direct mode, or the CH32 UART RX signal in bridge mode
- Module U1 pin 4: `GND`; share ground with the ESP32 or CH32 bridge
- Module U3 pin 1: `RB` (Ready/Busy status output; high = synthesizing/playing, low = idle)
- Module U3 pin 2: `GND`
- `RST`: active-low chip reset, handled by the on-board 1 MOhm/100 nF RC network and not exposed on U1
- Speaker output: on-board SPK1, 8 ohm / 0.5 W, connected across `BP0` and `BN0`

## Hardware Notes
- Module supply: 3.3 V, as shown by the supplied module schematic
- Chip operating-voltage specification: 2.4-5.1 V; the datasheet recommends 3.3-4.2 V and advises against operation above 4.5 V or below 3.0 V. These chip limits do not change the supplied board's 3.3 V project requirement.
- Power-up stabilization time: wait for the `0x4A` initialization-success byte or use a bounded startup timeout of [待确认]
- Enable pin: none
- Reset: SYN6288E `RST` is active low; no reset control is exposed on the four-pin module connector
- Crystal: on-board 16 MHz crystal with two 20 pF load capacitors
- Audio output: the chip contains a 10-bit push-pull DAC and the module directly connects an 8 ohm / 0.5 W speaker between BP0 and BN0
- Power budget: the chip datasheet reports about 3.3 mA when active but not playing and about 190 mA while playing text at volume 16 with a 3.0 V test supply. The project supply must tolerate speech-current transients; required board-level margin is [待确认].
- Encoding support: GB2312, GBK, BIG5, and Unicode; the encoding field in each synthesis frame must match the actual text bytes
- Status bytes: `0x4A` initialization success, `0x41` valid frame accepted, `0x45` frame rejected, `0x4E` synthesizing/playing, `0x4F` idle/playback complete
- Ready/Busy polarity: high while synthesizing/playing, low while idle and ready to receive data
- Timing: adjacent bytes within one frame must be separated by no more than 8 ms; separate frames by more than 8 ms. After receiving playback-complete `0x4F`, delay about 1 ms before the next continuous-playback frame.
- Sleep/wake: the chip sleeps only after the Power Down command. Any command on RxD can wake it; wait 16 ms after wake before sending the actual command frame.
- Baud-change timing: after sending the baud-change frame, wait 16 ms before another command and wait several hundred milliseconds before changing the host UART baud. The setting must be repeated after every reset.
- Reliability note: 9600 and 19200 bps are documented as stable. At 38400 bps, reception while the chip is already synthesizing is documented as unreliable; the project therefore remains at 9600 bps.
- Playback replacement: a new valid synthesis frame received during playback immediately stops the current text and starts the new text. Queue or reject overlapping requests when interruption is not intended.
- Known issue: transport completion only proves that bytes were delivered to the downstream UART. It does not prove that the SYN6288E accepted the frame or produced audible speech; use chip status feedback when confirmation is required.

## Identification Method
- Protocol identification: on a configured SYN6288E UART candidate route, send status-query frame `FD 00 02 21 DE`; expect `0x4E` (playing) or `0x4F` (idle)
- Power-up evidence: `0x4A` indicates SYN6288E initialization success
- Command acceptance evidence: `0x41` means a valid command frame was accepted; `0x45` means the command frame was rejected
- UART has no address. A route becoming available or a CH32 UART gateway receiving a dynamic node ID does not identify its downstream device as SYN6288E.

## Bridge Constraints
- Bridge timeout: 2000 ms (project default, twice the 1000 ms direct UART transmit timeout)
- Required constraints: the stable `ch32_uart_dynamic_node_t` reference is allocated and maintained by the discovery layer and passed into the SYN6288E driver config; the module driver must not discover gateways or assign node IDs. Use `ch32_uart_dynamic_gateway_final` for the complete START -> START_ACK -> DATA -> COMPLETE_ACK transfer. The shared CAN core is transitive infrastructure and must never be called directly by the module driver.
- UART profile: CH32 downstream UART TX/RX at 9600 bps, 8N1, ordinary non-inverted external UART levels; the supplied SYN6288E board performs its required RX inversion in hardware
- Payload constraints: preserve each SYN6288E command frame as one contiguous downstream UART write. Do not expose CAN fragment boundaries to the chip. Enforce the SYN6288E 206-byte complete-frame and 200-byte text limits even if the generic gateway accepts larger transfers.
- Timing constraints: the CH32 must emit the reassembled UART frame without inter-byte gaps over 8 ms. ESP32-side CAN fragmentation time is not a SYN6288E UART inter-byte gap because the CH32 transmits only after reassembly.
- Response handling: route CH32 UART RX uplink bytes back to the module/session that owns the stable node. Distinguish gateway COMPLETE_ACK, SYN6288E `0x41` acceptance, `0x45` rejection, `0x4E` busy, and `0x4F` playback completion.
- Retry/idempotency: do not blindly retry a synthesis frame after an ambiguous timeout, because the chip may already be speaking and a repeated synthesis frame restarts playback. Query status or apply an application-level request policy before retrying.
- Recommended implementation: maintain a bounded fixed-capacity request queue, serialize synthesis frames, and wait for `0x4F` when uninterrupted sequential speech is required

## Dependencies
- Depends on: none
- Typically used with: ESP32-WROOM, CH32V203 UART bridge (bridge mode)

## Software Package Paths
- Direct driver: `components_direct/syn6288e_direct/`
- Direct example: `examples_direct/syn6288e_direct_test/`
- Bridge driver: `components_ch32/ch32_syn6288_gateway/`
- Bridge example: `examples_ch32/syn6288_ch32_test/`

## Transport Dependencies
- Direct BSP: `bsp_uart` + `bsp_board`
- Bridge protocol layer: `ch32_uart_dynamic_gateway_final`
- Shared CAN core: `ch32_can_gateway_core` (transitive only; never called by the module driver)
- Required CH32 firmware: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/`

## Identification Confidence
- WHO_AM_I available: no
- Address-only identification: not applicable; UART has no device address
- Protocol confirmation: status query `FD 00 02 21 DE` returning `0x4E` or `0x4F`; power-up `0x4A` and command response `0x41` provide additional evidence
- Manual configuration required: yes for initially selecting a candidate UART route; protocol response then confirms SYN6288E-compatible behavior
- Exact-model limitation: the protocol response confirms a SYN6288/SYN6288E-compatible endpoint but does not independently distinguish package revision SYN6288E from every protocol-compatible predecessor

## Authoritative Sources
- Chip datasheet: `C:/Users/chenqing/Desktop/第一批模块资料/文本转语音/chip datasheet_tts.pdf` - authoritative for SYN6288E protocol, status bytes, UART format, frame limits, encoding, timing, electrical characteristics, reset and audio behavior
- Module schematic: `C:/Users/chenqing/Desktop/第一批模块资料/文本转语音/SCH_tts.pdf` - authoritative for the supplied board's 3.3 V supply, four-pin UART connector, on-board RX inversion, reset network, Ready/Busy header, crystal and speaker wiring
- CH32 firmware: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/main.c`
- Verified ESP32 direct example: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM/examples_direct/syn6288e_direct_test/`
- Verified ESP32 bridge example: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM/examples_ch32/syn6288_ch32_test/`
- Hardware validation date/result: direct and CH32-bridge speech output were previously reported working; exact validation date [待确认]

## Source Conflicts and Resolution
- The module title block uses the legacy text `SYN6288`, while schematic U2 and the supplied chip datasheet identify the installed/current chip as `SYN6288E`. Use `SYN6288E` as the module model and abbreviation.
- The chip-level datasheet says host-to-chip UART data must be inverted. The module schematic already supplies this inversion between external `RX` and chip `RXD_6288`; therefore the external module interface uses normal UART polarity.
- The datasheet permits a wider chip supply range, but the supplied module schematic labels all external and chip supply rails as 3V3. Use 3.3 V for this module.

## Code Implementation Guide

### Operating Model
- Module behavior: command-driven UART text-to-speech output with asynchronous
  chip status bytes.
- Mental model: the driver validates text encoding/length, creates one complete
  `0xFD` synthesis frame with XOR checksum, serializes it to the module and
  interprets chip responses separately from transport acknowledgements.
- Initialization prerequisite: normal non-inverted external 9600 8N1 UART,
  stable 3V3/audio supply and bounded wait for optional power-up `0x4A` evidence.

### Initialization Sequence
1. Initialize the external UART at 9600 8N1 with normal polarity; do not apply
   a second inversion because the module transistor already provides it.
2. Drain stale receive bytes and wait within a bounded startup window for `0x4A`
   when RX feedback is available.
3. Send the status query and accept `0x4E`/`0x4F` as protocol evidence before
   identifying a candidate route as SYN6288E-compatible.
4. Initialize the fixed request/receive state and mark the instance ready/idle
   according to the latest chip status.
- Order constraints: UART configuration precedes protocol identification;
  gateway assignment alone never proves the downstream module identity.

### Runtime Data or Command Path
```text
text request -> validate encoding and <=200-byte text -> build complete frame ->
XOR checksum -> serialized UART/gateway write -> transport completion ->
chip 0x41/0x45 acceptance -> optional 0x4F playback completion
```
- Blocking points: bounded UART/gateway send and optional bounded chip-response
  wait. Playback itself must not block unrelated sampling/display tasks.
- Completion evidence: direct UART write or gateway COMPLETE_ACK proves byte
  delivery only; `0x41` proves frame acceptance; `0x4F` proves chip playback
  completion, while audible sound still requires hardware observation.

### Data Conversion and Validity
- Raw format: big-endian data-area length, command/encoding/text bytes and XOR
  checksum; total frame <=206 bytes and text <=200 bytes.
- Conversion: input text must be encoded to the encoding declared in the frame;
  bytes and length are computed after encoding, not from character count.
- Validity checks: non-null payload, supported encoding, length bounds, checksum,
  expected response byte and current busy/request policy.
- Invalid behavior: reject locally before transmit where possible; map `0x45` to
  frame-rejected without treating the UART route itself as offline.

### Timing and Scheduling Characteristics
- Power/config delays: startup timeout [待确认], 16 ms after wake, documented
  baud-change waits and about 1 ms after `0x4F` for continuous playback.
- Command timing: UART bytes of one frame must not be separated by more than
  8 ms; separate frames by more than 8 ms.
- Recommended effective rate: 9600 bps is the project rate; application speech
  cadence follows requested minimum action gaps and actual playback policy.
- Periodic schedule or minimum action gap: speech/alarms use a minimum gap timed
  from previous action completion/success. Do not use catch-up periodic timing.
- Work that can proceed while waiting: other FreeRTOS tasks continue; only this
  speech instance/request state is serialized.

### Lifecycle and Recovery
```text
uninitialized -> uart ready -> identifying -> idle -> transmitting -> accepted -> playing -> idle
```
- Transient failure: bounded retry is allowed only when it cannot duplicate an
  already accepted speech action.
- Ambiguous timeout: query status or apply the application request policy; do
  not blindly resend because a repeated frame interrupts/restarts playback.
- Consecutive route failure: mark only this instance offline and let the owner
  rediscover/rebind the candidate route.
- Frame rejection: keep the transport online, report protocol failure and drop
  or correct that request.

### Direct Implementation Notes
- BSP/API boundary: use current `bsp_uart`/`bsp_board`; public module API exposes
  speech requests/status rather than ESP-IDF UART types.
- Instance/buffer model: static/fixed pool with a fixed maximum frame buffer and
  bounded receive/status state; no allocation per utterance.
- Polling/callback/task consideration: receive/status parsing may be polled or
  fed by a bounded UART task; a separate blocking-output consumer is appropriate
  in compositions because speech duration and acknowledgement are independent.
- Public API should distinguish transmitted, accepted, rejected, busy and
  playback-complete states where RX feedback exists.

### CH32 Bridge Implementation Notes
- Required generic gateway: `ch32_uart_dynamic_gateway_final` only.
- Stable-node and live-presence validation: validate F2-confirmed UART node and
  confirm the downstream protocol through returned status bytes when supported.
- Transfer expansion: preserve one SYN frame as one logical gateway write; CH32
  reassembles CAN fragments before emitting UART bytes so chip inter-byte timing
  remains within specification.
- Timeout/idempotency: distinguish START/DATA/COMPLETE acknowledgement from chip
  acceptance, and do not retry ambiguous synthesis frames blindly.
- Existing generic capability assessment: full START/DATA/ACK transport and RX
  uplink are recorded as available.
- Discovery/business boundary: gateway discovery owns route identity/lifetime;
  module driver owns SYN framing/status; application owns wording and cadence.

### Module-Specific Difficulties and Common Mistakes
- Distinguishing characteristic: transport completion, chip acceptance,
  playback completion and audible output are four different evidence levels.
- Likely mistake: applying software UART inversion in addition to the module
  transistor, producing invalid data.
- Likely mistake: using character count as payload byte length for GBK/Unicode.
- Likely mistake: periodic catch-up or blind retry can produce rapid repeated or
  interrupted speech; enforce completion-based minimum gaps and ambiguity policy.
- Hardware validation focus: power transient stability, actual audible output,
  status-byte reception, long text boundaries and overlapping request behavior.

### Implementation Complexity
- Direct implementation: medium.
- CH32 bridge implementation: high.
- Main complexity sources: text encoding, bounded frame construction, response
  state machine, ambiguous retry semantics and bridged RX routing.
- Reusable existing capabilities: verified direct/bridge drivers, current UART
  BSP, dynamic UART gateway and fixed-capacity conventions.
- Possible generic capability gaps: none currently confirmed; hardware feedback
  depends on the physical TX/RX wiring and route firmware.

### Guide Evidence Classification
- Confirmed from user material: SYN6288E model, 3V3 module, normal external
  9600 8N1 UART, frame/status formats, onboard inversion and audio wiring.
- Supplemented from official sources: limits, timing, status and playback
  replacement behavior in the supplied chip datasheet.
- Inference: bounded output consumer, completion-based cadence and local recovery
  follow the documented behavior plus project multi-module rules.
- Pending hardware verification: exact startup timeout, board supply margin and
  whether every deployed CH32 route returns all chip status bytes reliably.

## Module-Specific Error Codes
- `ERR_SYN6288E_FRAME_REJECTED` (-10): chip returned `0x45` for an unrecognized or invalid command frame
- `ERR_SYN6288E_PROTOCOL` (-11): received an unexpected SYN6288E response or a response inconsistent with the current transaction state
- Transport timeout, invalid parameters, missing initialization, busy state and hardware faults use the common error codes defined in `_global/error-codes.md`

## Changelog
- 2026-08-12: Initial module card created from the supplied SYN6288E chip datasheet, module schematic, project global rules, module-card template, board classification, and verified SDK path inventory
- 2026-08-12: Added the reusable Code Implementation Guide with separated transport/acceptance/playback evidence and completion-based action timing
