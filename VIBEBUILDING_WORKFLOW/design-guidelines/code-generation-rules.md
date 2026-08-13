# AI Code Generation Rules

This file is a mandatory reference for AI when generating any ESP32 multi-module code. Violating any of these rules produces incorrect code.

## RULE 1: Never Hardcode Connection Mode

WRONG:
- "If local UART is found, assume it's SYN."
- "If local I2C has TOF, don't scan CH32-side TOF."

CORRECT:
- Scan both direct and CH32-bridged sides.
- Multiple instances of the same device type are allowed.
- UART route != UART device. Must confirm via protocol evidence or explicit config.

## RULE 2: No Single Module Failure May Crash the System

Each device has its own state: present / ready / valid / ok_count / error_count.

- One device's init failure only marks itself failed.
- One device's read timeout only affects its own error_count.
- The main loop continues running all other devices.
- A node with no downstream devices (empty CH32) must not block discovery of other nodes.

## RULE 3: Rediscovery Must Be Incremental

- Rediscovery places new results in a temporary container.
- Match shared gateway nodes by `device_type + token` (token alone only in an already type-separated table): update in-place, don't delete.
- Match devices by kind+link+addr+node-reference: update status, don't recreate.
- On rediscovery failure or timeout: keep the old tables. System continues.
- Never clear-all-and-rebuild: a CAN transient error must not wipe the device table.
- UART rediscovery: never send global F3 RELEASE (shared CAN ID 0x000 would nuke working I2C nodes).
- Runtime cadence follows `device-discovery.md`: immediate startup discovery,
  then 10-second incremental rediscovery while any required role is missing and
  30-second incremental rediscovery while all required roles are online.

## RULE 4: I2C and UART Identification Differ Fundamentally

- I2C: address IS the identification clue (e.g. 0x29 -> probably VL53L0X, 0x3C -> probably OLED). Supplement with WHO_AM_I.
- UART: no address. Must use protocol content:
  - VC02: recognized by received command strings (e.g. "TX WK 00", "TX CL OL ED", "TX RF OL ED").
  - SYN6288: recognized by status response frame. If no response (TX-only bridge), stays as candidate route.
  - Without evidence: call it "UART candidate route", NOT "SYN device".

## RULE 5: Dynamic Allocation Must Match Real CH32 Firmware

AI must not invent protocols based on old examples. Current facts:

- Both I2C and UART gateways use CAN ID `0x000` for F0 discovery.
- device_type: 0x01=I2C, 0x03=SPI, 0x04=UART.
- token is in data[5..6], little-endian.
- Current verified I2C and UART dynamic gateways both use F1 node_id in data[1], token in data[4..5], device_type in data[6], and protocol_version in data[7].
- UART node_id starts at 0x31 (not 1). I2C starts at 1. SPI starts at 0x21.
- CAN ID mapping: I2C cmd=0x200+node, status=0x100+node, ack=0x500+node, hello=0x700+node. UART adds DATA=0x300+node, RX=0x400+node.
- UART transport uses START/DATA/ACK with CRC16. Never bypass START_ACK.
- I2C legacy reads are single-frame only when request_id=0 and len<=3. Preferred ESP32 code uses a non-zero request_id and collects STATUS_READ_CHUNK + STATUS_READ_DONE for every length.
- OLED uses WRITE_MULTI, not WRITE_RAW.
- A node becomes ready only after a matching F2 (type + token + assigned node_id). HELLO is health evidence, not assignment confirmation.
- Shared discovery identity is `device_type + token`; node_id is only a mutable runtime route.
- Multi-CH32 I2C discovery must retry the zero-token inventory query with changing sequence. F0/F1/F2 retain shared ID 0x000 to match the immutable UART/common-node protocol; I2C uses token-based response slots to recover from same-ID collisions.

Before modifying ESP32 protocol code, verify against the corresponding CH32 `main.c`. Do not rely solely on historical logs or old examples.

## RULE 6: OLED Refresh Must Be Efficient, Not Artificially Slow

An OLED can consume substantial transport bandwidth, especially through a
CAN-CH32-I2C bridge.  Throttling means eliminating waste; it does not mean
choosing a visibly slow refresh rate.

- Select the highest stable effective refresh rate supported by the module,
  downstream bus, CH32 firmware, CAN fragmentation and other active traffic.
- Do not copy a slow delay from a reference example.  Derive the period from
  the current observable requirement and a transaction/bandwidth budget.
- Refresh periodically and only when display content actually changes.
- Use WRITE_MULTI for bulk writes, reducing CAN round-trips.
- Separate "clear screen" from "refresh". Voice "clear screen" should suspend automatic refresh.
- Use page/region caching: compare old and new frame, then send only changed
  pages or regions.  Normal operation must not clear or transmit the whole
  framebuffer merely to replace a short value.
- Format changing text to a fixed field width, or clear only its old region,
  so shorter text does not require a full-screen clear.
- Job queue depth = 1: always keep only the latest display request and discard
  obsolete intermediate frames.  Identical content must not be queued again.
- If one refresh takes longer than the requested period, reduce bytes and
  round-trips before lowering the user-visible refresh rate.

## RULE 7: Voice Broadcast Must Not Block Main Loop

SYN broadcast constraints:

- Danger state: broadcast at intervals (e.g. 1 second), not every loop iteration.
- If a UART route fails: give it a cooldown period. Don't retry every cycle.
- If local UART is occupied by VC02: never send SYN frames to it.
- UART bridge TX must execute the full START_ACK -> DATA fragments -> COMPLETE_ACK sequence.
- Timeout at any stage affects only that route. Must not block sensor and OLED tasks.
- "At most once per second" is a minimum action gap, not a fixed sampling
  schedule.  Measure it from the completion (or documented success point) of
  the previous broadcast.  Do not use catch-up scheduling that can issue
  back-to-back broadcasts after a blocking transaction.

## RULE 8: VC02 Identification Pattern

VC02 is an input device that actively reports recognized voice commands via UART. Known command strings:

- "TX WK 00" — wake-up
- "TX CL OL ED" — clear OLED
- "TX RF OL ED" — refresh OLED
- "TX SA VL 00" — save volume
- "TX SO SY NO" — stop SYN
- "TX SA SY NO" — start SYN

Once any of these strings is received on a UART route, that route is confirmed as VC02. No other device type (SYN, unknown) should be broadcast to that route.

## RULE 9: ESP32 CH32-Side Hardware Requirements (I2C example)

When AI generates ESP32 code that communicates with CH32 gateways, these must match:

1. All command codes, status codes, CAN ID mappings consistent with CH32 firmware.
2. ID assignment flow logic matches: F0 listen -> F1 assign with token echo -> F2 confirm.
3. I2C reads: legacy request_id=0 and len<=3 may use STATUS_READ; preferred non-zero request_id must collect STATUS_READ_CHUNK + STATUS_READ_DONE.
4. Config register writes use WRITE_REG (0x03).
5. Bulk writes use WRITE_MULTI (0x08), split into payloads of at most 4 bytes with START/END flags; wait for matching status and ACK for every fragment.

Most common pitfalls:
- Token not echoed back -> ID assignment fails.
- CAN ID mapping mismatch -> communication fails.
- Not waiting for all chunks -> incomplete data.
- WRITE_RAW len calculation error -> command rejected.
- Logs fail to distinguish: discovery failure, scan failure, init failure, read failure.

## RULE 10: Logging Must Distinguish Failure Types

In multi-module code, logs must distinguish these failure stages:
- Discovery failure: CAN timeout, no F0 received.
- Scan failure: gateway assigned but downstream bus scan returned nothing.
- Init failure: device detected but WHO_AM_I mismatch or config write failed.
- Read failure: device previously working, now timed out.

Each uses a different log message so the human reader can locate the fault without source code.

## RULE 11: One Transport-Neutral CAN Owner

- `ch32_can_gateway_core` is the only ESP32 component that may install/start TWAI or call `twai_receive()`.
- I2C and UART gateway components depend on the core directly and must not depend on each other.
- Multi-frame transfers use the shared core transaction lock so I2C and UART frames cannot interleave inside one logical transaction.
- A module driver depends on its transport protocol layer, not directly on TWAI and not directly on the CAN core.

## RULE 12: Complete Single-Module Packages, Exact Multi-Module Reuse

- A new single-module task generates a complete independent driver package and
  a complete independent example package for the requested mode.
- A concrete module driver must not depend on another concrete module driver.
  Existing drivers may be read for structure, BSP/gateway use, logging and
  error handling only.
- A multi-module example reuses a verified module driver only if it works
  unchanged. If registers, protocol, initialization, cfg, public API or module
  behavior must change, first create and verify a complete new driver.
- Do not partially wrap, subclass or cross-link concrete module drivers.

## RULE 13: Read Real APIs; Never Invent Bridge Helpers

- Before generation, read the target repository's current public headers and
  component `CMakeLists.txt` files.
- Direct implementations use the actual BSP API. Public module headers should
  not expose ESP-IDF `driver/*` types unless unavoidable and explicitly agreed.
- I2C bridge modules call `ch32_i2c_multi_gateway_final`; UART bridge modules
  call `ch32_uart_dynamic_gateway_final`.
- Do not generate `can_bridge.h`, `can_send_gpio_write()`,
  `can_send_pwm_duty()` or any other helper absent from the target repository.
- If a bridge capability is missing, report it and implement/verify the CH32
  firmware and ESP32 protocol layer before generating the module driver.

## RULE 14: Discovery Queues Are Type-Isolated

- F0/F2/HELLO frames must be routed to independent I2C/UART discovery queues by
  their actual device_type field. Protocol layers must not race on one common
  observer queue.
- `0x100+node` I2C status, `0x400+node` UART uplink and `0x500+node` ACK frames
  are routed using their real protocol layouts.
- Do not treat UART ACK `data[3]` as device_type; current UART ACK phases are
  `0x30`..`0x34` and `data[3]` is result.

## RULE 15: Fixed Reference Packages by Transport

For every new I2C or UART module, the following verified packages are mandatory
implementation references.  AI does not choose a different concrete module as
the primary reference merely because its function appears more similar.

### Direct I2C module

Read and follow the SSD1315 direct package for BSP use, public-header boundary,
component/CMake organization, initialization/error structure and complete
minimum-example organization:

```
components_direct/ssd1315_direct/
examples_direct/ssd1315_direct_test/
```

### Direct UART module

Read and follow the SYN6288E direct package for `board.h`/`bsp_uart` use,
public-header boundary, component/CMake organization, UART timeout/error
structure and complete minimum-example organization:

```
components_direct/syn6288e_direct/
examples_direct/syn6288e_direct_test/
```

### CH32-I2C module

Read and follow the SSD1315/OLED bridge chain for shared-core ownership,
dynamic ID, stable-node lifetime, I2C gateway API use, failure-stage logging,
component dependencies and bridge-example organization:

```
components_esp32wroom/ch32_can_gateway_core/
components_esp32wroom/ch32_i2c_multi_gateway_final/
components_ch32/ch32_ssd1315_gateway/
examples_ch32/ssd1315_ch32_test/
```

### CH32-UART module

Read and follow the SYN6288E bridge chain for shared-core ownership, dynamic
ID, stable-node lifetime, UART START/DATA/ACK gateway use, failure-stage
logging, component dependencies and bridge-example organization:

```
components_esp32wroom/ch32_can_gateway_core/
components_esp32wroom/ch32_uart_dynamic_gateway_final/
components_ch32/ch32_syn6288_gateway/
examples_ch32/syn6288_ch32_test/
```

The reference scope is architecture and API usage only.  Module-specific
registers, command bytes, initialization sequence, identification method,
timing, retry policy, payload semantics and observable behavior always come
from the new module's `card.md`, datasheet and authoritative peer firmware.
Never copy SSD1315 framebuffer/display logic into another I2C module or
SYN6288E speech frames/GBK/timing into another UART module.

## RULE 16: Fast Generation and Human-First Compilation

The default code-generation handoff is **source generation plus static checks**.
Compilation, flashing and serial monitoring are not default generation steps.

### Required during every generation task

- Generate the complete driver, public header, CMake files, complete minimum
  example and README required by the task.
- Read current public headers, relevant implementations and CMake files before
  using an API. Do not trade correctness for speed.
- Perform fast static checks: required files exist; component names and
  dependencies match; forbidden headers/APIs are absent; bridge drivers do not
  own discovery or CAN; direct public headers do not expose forbidden
  `driver/*` types; requested observable behavior is present.
- Report the exact manual build command and explicitly mark compile, flash,
  runtime discovery and hardware behavior as `pending human verification`.

### Default actions that AI must not perform

- Do not run the first full ESP-IDF build after generating code.
- Do not run `set-target`, `fullclean`, flash, monitor or serial capture.
- Do not create build-output directories merely to claim validation.
- Do not treat an uncompiled package as compiled or hardware-verified.

### When compilation is allowed or required

- If the current user request explicitly asks AI to compile, flash or monitor,
  that explicit instruction overrides this default for the named action.
- The human normally performs the first compile. When the human returns build
  errors and asks AI to correct them, AI fixes the source and then runs an
  incremental compile to verify the correction, unless the human says not to.
- Prefer `idf.py build` in the existing configured project after a correction.
  Avoid `fullclean` or repeated `set-target` unless the target/configuration is
  actually wrong or the human explicitly requests a clean build.
- When a requested build is necessarily long, run it once and inspect its real
  log/exit status. Do not launch duplicate builds because a tool response
  window expired.

### Efficient reference reading

- Never recursively ingest `build/`, managed-component caches, generated
  `sdkconfig` artifacts or binary outputs as reference source.
- Start with the precise entry files listed in `sdk-reference.md`: public
  header, component CMake, relevant implementation function, reference example
  `main.c`, and authoritative peer firmware capability.
- Search for the required API/capability first, then read the containing
  function and its protocol peer. Do not dump an entire repository tree when a
  bounded lookup can prove the same fact.
- Reuse the verified capability summary in `sdk-reference.md`; re-open the full
  implementation only when the current repository differs, the needed
  operation is absent from the summary, or sources conflict.

## RULE 17: Multi-Module Examples Are Reusable FreeRTOS References

A multi-module example is primarily a reference implementation for another AI.
It must realize the requested phenomenon, but must not trade reusable boundaries
for a one-off monolithic solution.

### Default execution model

- Use FreeRTOS by default for multi-module examples.
- AI decides task boundaries from execution mechanics: module count, timing,
  blocking behavior, shared transport, minimum action gaps, recovery isolation,
  and producer/consumer data flow.
- Do not mechanically create one task per module, and do not place every module
  in one serial super-loop.  Short operations with the same cadence may share a
  task; independently blocking or independently recoverable operations should
  be separated.
- Discovery/session maintenance is separate from normal module behavior.
  Sensor sampling produces a state snapshot; display, speech and other outputs
  consume that snapshot without embedding their business inside the sensor
  driver.
- Prefer a latest-state snapshot, depth-one queue, event bits or another
  bounded mechanism.  Do not accumulate stale display/status work.

### Neutral scheduling

- Unless card.md, the user, or an explicit hardware real-time constraint says
  otherwise, application tasks use the same FreeRTOS priority.
- Do not infer task priority from module purpose, visibility, names such as
  "critical", or subjective importance.  Use neutral names such as sampling,
  state consumer, blocking output, discovery maintenance and recovery.
- Obtain responsiveness through bounded blocking, short lock scope, timeouts,
  state coalescing and adaptive maintenance periods, not by making one module
  outrank another.

### Periodic work versus minimum action gaps

- Stable periodic sampling may use `vTaskDelayUntil()`.
- An action constrained as "no more than once per N milliseconds" is timed
  from the previous action completion/success and must not catch up missed
  periods.  Speech, alerts, relays and similar side effects follow this rule.
- Failed side effects use a documented cooldown/backoff and do not retry every
  producer cycle.

### Shared-resource boundaries

- A mutex covers only the shared transport transaction or the smallest state
  update that requires mutual exclusion.
- Do not hold a bus/gateway mutex while formatting text, waiting for the next
  period, updating unrelated application state, or executing an entire long
  discovery-and-initialize round.
- Split discovery, stable-table merge, per-device probe and per-device init
  into bounded operations.  Every error path releases its lock.
- A slow display, speech transaction or missing module must not alter the
  sampling cadence or stop other online modules.

### Module lifecycle and local recovery

- Represent equivalent lifecycle states for each instance, such as unbound,
  initializing, online, degraded and offline; exact enum names are optional.
- A module failure affects only that module.  Recover/rebind it locally without
  clearing stable nodes, reinitializing healthy modules, restarting the shared
  CAN core or rebooting ESP32.
- Repeated identical failure logs are rate-limited; state transitions and
  recovery success are logged once when they occur.

### Internal runtime budget before writing code

Before generation, AI must reason internally about:

- requested roles and module instances;
- task grouping and every potentially blocking operation;
- sample, output, minimum-gap and rediscovery periods;
- shared gateway/lock boundaries;
- bytes, fragments and round-trips per display or bulk update;
- timeout/retry worst cases and whether stale work can accumulate;
- local recovery behavior when one role is absent or fails.

This analysis normally does not create an intermediate document.  Optimize in
this order: remove duplicate transfers, send only changed content, coalesce
updates, shorten lock scope, decouple blocking operations, discard obsolete
states, then raise the effective update rate within measured transport limits.
Reducing the requested user-visible rate is the last resort.

## RULE 18: Do Not Expand the Knowledge Base Without Authorization

- Treat the knowledge-base root as read-only during ordinary code generation,
  compilation, debugging, research, flashing and serial-monitor tasks.
- The sole normal creation exception is
  `modules/<module>/card.md` in an explicitly requested Stage 1 new-module task.
- Direct drivers/examples, CH32 bridge drivers/examples, multi-module example
  code, English source code, CMake files, build output, logs and temporary work
  belong in the user-specified external repository, never in this knowledge base.
- Edit an existing knowledge file only when the user explicitly requests a
  knowledge-base update/correction or ingestion of confirmed validation facts.
- New guidelines, prompts, templates, composition documents, validation files,
  summaries, manifests, checklists, helper scripts and archive entries require
  separate explicit authorization. Do not create them as helpful intermediate
  artifacts.
- If a task reveals a reusable rule or correction but no update was authorized,
  describe the proposed knowledge change in chat and wait. Do not write it.
- Never create `.tmp*`, `_tmp*`, `tmp`, `staging*`, `revision*`, `build*`,
  `sdk_output` or equivalent work directories under the knowledge-base root.
