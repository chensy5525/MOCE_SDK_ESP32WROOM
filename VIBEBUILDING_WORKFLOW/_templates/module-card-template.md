# <Module Name> Card

## Hardware Model
<chip/module full model number>

## Module Type
<sensor-data / sensor-event / actuator / driver / communication / hmi-input / hmi-output>

## Communication Interface
<Interface: I2C / UART / SPI / GPIO / PWM / ADC>, <Connection: direct / bridged>

## Comm Parameters

### I2C
- Device address: 0x<val> (<7-bit/8-bit>, <address variability note>)
- Max speed: <val> kHz

### UART
- Factory default: <baud> bps, <data>N, <stop>, <parity>
- Project target: <baud> bps
- Flow control: <yes/no>

### SPI
- Mode: <0/1/2/3> (CPOL=<val>, CPHA=<val>)
- Max clock: <val> MHz
- CS pin: GPIO_<num>

### GPIO/PWM
- Pin function descriptions
- PWM freq: <val> Hz, resolution: <val> bit

## Pin List
- PIN_<ABBR>_<FUNC> = GPIO_<num>
- PIN_<ABBR>_EN  = GPIO_<num>  Active level: <high/low>
- PIN_<ABBR>_RST = GPIO_<num>  Reset sequence: pull low <val> ms

## Hardware Notes
- Power-up stabilization time: <val> ms
- Enable pin: <note>
- Range / thresholds: <note>
- Special timing requirements: <note>
- Independent power supply: <note>
- Known issues: <note>

## Identification Method
- Register identification: read reg 0x<addr> -> expect 0x<val>
- Protocol identification: send <command> -> expect response containing <string>
- Port-mapped identification: requires manual config

## Bridge Constraints
- Required constraints: timeout <val> ms (or direct x multiplier), protocol
  operation/payload limits, retry cursor/idempotency and required bus rate
- Recommended implementation: caching, throttling, request coalescing or "none"

## Dependencies
- Depends on: <list of module abbreviations, or "none">
- Typically used with: <list, or "none">

## Software Package Paths
- Direct driver: `components_direct/<module>_direct/`
- Direct example: `examples_direct/<module>_direct_test/`
- Bridge driver: `components_ch32/ch32_<module>_gateway/`
- Bridge example: `examples_ch32/<module>_ch32_test/`

## Transport Dependencies
- Direct BSP: <bsp component or "none">
- Bridge protocol layer: <I2C/UART/SPI/GPIO protocol component or "not implemented">
- Shared CAN core: `ch32_can_gateway_core` (transitive only; never called by module driver)
- Required CH32 firmware: <authoritative path or [待确认]>

## Identification Confidence
- WHO_AM_I available: <yes/no>
- Address-only identification: <yes/no and ambiguity>
- Protocol confirmation: <command/response or none>
- Manual configuration required: <yes/no>

## Authoritative Sources
- Datasheet: <path/document or [待确认]>
- Schematic: <path/document or [待确认]>
- CH32 firmware: <path or not applicable>
- Verified ESP32 example: <path or [待确认]>
- Hardware validation date/result: <result or [待确认]>

## Code Implementation Guide

This section is a reusable implementation explanation, not source code.  It
must be understandable by a user who has never used the module and concrete
enough for the direct and CH32-bridge generation stages to follow without
re-researching the module from scratch.

### Operating Model
- Module behavior: <single-shot / continuous / command-driven / active-report / event-driven>
- Mental model: <plain-language explanation of how the module produces data or performs actions>
- Initialization prerequisite: <power/reset/calibration/mode prerequisite>

### Initialization Sequence
1. <ordered initialization step and why it is required>
2. <presence/identity confirmation>
3. <configuration/calibration/mode setup>
4. <transition to ready state>
- Order constraints: <steps that must not be reordered or omitted>

### Runtime Data or Command Path
```text
<trigger/input> -> <wait/status> -> <transfer> -> <validate> ->
<convert/act> -> <return/ack>
```
- Blocking points: <operations that may block and their bounded timeout>
- Completion evidence: <what transport success and physical success each prove>

### Data Conversion and Validity
- Raw format: <byte order, width, sign, channels>
- Conversion: <formula, scale, offset, calibration source and output unit>
- Validity checks: <status bits, ranges, sentinel values, checksum>
- Invalid/out-of-range behavior: <error/result policy; never silently clamp unless specified>

### Timing and Scheduling Characteristics
- Power-up/config delays: <values and sources>
- Measurement/update/command timing: <values and sources>
- Recommended effective rate: <rate justified by module and transport capability>
- Periodic schedule or minimum action gap: <which semantic applies>
- Work that can proceed while waiting: <note>

### Lifecycle and Recovery
```text
<uninitialized> -> <initializing> -> <ready/online> -> <active state>
```
- Transient failure: <bounded retry behavior>
- Invalid sample/event: <effect on this sample versus device online state>
- Consecutive communication failure: <local offline/reinit behavior>
- Unrecoverable identity/config failure: <behavior>

### Direct Implementation Notes
- BSP/API boundary: <actual target BSP family to verify in Stage 2>
- Instance/buffer model: <static instance/fixed pool and required buffers>
- Polling/callback/task consideration: <mechanical reason, without task-importance wording>
- Public API should expose: <transport-neutral result/control data>

### CH32 Bridge Implementation Notes
- Required generic gateway: <I2C/UART/SPI gateway>
- Stable-node and live-presence validation: <requirements>
- Transfer expansion: <CAN round-trips, fragmentation, bulk-write/read needs>
- Timeout/idempotency considerations: <requirements>
- Existing generic capability assessment: <sufficient / missing API [待确认]>
- Discovery/business boundary: <what remains outside the module driver>

### Module-Specific Difficulties and Common Mistakes
- Distinguishing characteristic: <what makes this module unlike a basic register read/write device>
- Likely implementation mistake: <mistake -> consequence -> prevention>
- Datasheet/revision ambiguity: <note or none>
- Hardware validation focus: <what software evidence cannot prove>

### Implementation Complexity
- Direct implementation: <low / medium / high>
- CH32 bridge implementation: <low / medium / high>
- Main complexity sources: <list>
- Reusable existing capabilities: <list>
- Possible generic capability gaps: <list or none>

### Guide Evidence Classification
- Confirmed from user material: <facts and source locations>
- Supplemented from official sources: <facts and links/document sections>
- Inference: <reasoned conclusions that are not explicit source facts>
- Pending hardware verification: <claims requiring a physical test>

## Module-Specific Error Codes
- ERR_<ABBR>_<MEANING> (<val>): <description>

## Changelog
- YYYY-MM-DD: <summary>


> **AI Fill Rules:**
> - All pin values, register addresses, and chip models MUST be extracted from user-provided materials
> - Fields that cannot be extracted: mark `[待确认]`
> - If a missing field can change hardware behavior or protocol compatibility,
>   stop code generation and request confirmation
> - Never fabricate any hardware parameter
> - When implementation facts are missing, perform a targeted search. Prefer
>   chip/module manufacturer datasheets, application notes and official SDKs;
>   third-party code is a clue, not protocol authority
> - Every key conclusion in "Code Implementation Guide" must be traceable to
>   user material or an authoritative source, or explicitly labeled inference
>   or pending hardware verification
> - Stage 1 writes the complete guide into card.md and also presents a concise,
>   user-readable implementation guide directly in the final chat response
> - Stage 1 still generates no driver or example source code
