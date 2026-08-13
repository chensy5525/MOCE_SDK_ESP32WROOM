# VL53L0X ToF Ranging Module Card

## Hardware Model
STMicroelectronics VL53L0X (confirmed on physical module by I2C model ID)

## Module Type
sensor-data

## Communication Interface
I2C, direct and bridged

## Comm Parameters

### I2C
- Device address: `0x29` (7-bit; datasheet 8-bit write address `0x52`)
- Maximum/project speed: 400 kHz
- Register index width: 8 bit
- Multi-byte values: big-endian

### UART / SPI / GPIO/PWM
- UART/SPI: not applicable
- XSHUT and GPIO1 are optional; direct example uses polling and only requires 3V3, GND, SDA and SCL

## Pin List
- `3V3`: regulated 3.3 V
- `GND`: common ground
- `SDA`: ESP32 GPIO 21
- `SCL`: ESP32 GPIO 22

## Hardware Notes
- Module supply: 3.3 V
- Default address: `0x29`
- Model identification: 8-bit register `0xC0` returns `0xEE`
- Nominal maximum range: 2000 mm; actual usable range depends on target reflectance and ambient light
- Target reporting rate: 5 Hz (one serial report every 200 ms)
- Out-of-range rule: distance above 2000 mm is `OUT RANGE`; communication and ready timeout are separate errors
- I2C retries: maximum 3 attempts
- Calibration: offset/crosstalk calibration for production accuracy [待确认]

## Target Observable Behavior
- Direct: ESP32 UART0 prints five distance results per second in millimetres; values above 2000 mm print `OUT RANGE`
- Bridged: same ESP32 UART phenomenon through the CH32 I2C transport

## Identification Method
- Read register `0xC0`; expect `0xEE`
- Address ACK at `0x29` alone is insufficient

## Bridge Constraints
- Use `ch32_i2c_multi_gateway_final`; do not call CAN/TWAI directly
- Preserve 8-bit register addressing and VL53L0X initialization/calibration ordering
- Maximum 3 transport attempts; never retry indefinitely

## Dependencies
- Direct driver: only `bsp_i2c` and platform timing support
- Existing concrete VL53L0X drivers are references only and are not dependencies

## Software Package Paths
- Direct driver: `components_direct/vl53l0x_direct/`
- Direct example: `examples_direct/vl53l0x_direct_test/`
- Bridge driver: `components_ch32/ch32_vl53l0x_gateway/`
- Bridge example: `examples_ch32/vl53l0x_ch32_test/`

## Identification Confidence
- WHO_AM_I: `0xC0 = 0xEE`
- Hardware validation: passed on 2026-08-12 using ESP32 COM11
- Observed distance: approximately 32–40 mm during validation

## Authoritative Sources
- Physical module I2C response and successful ranging validation (highest authority for this supplied unit)
- ST VL53L0X datasheet DS11555 and API manual UM2039
- STSW-IMG005 VL53L0X API
- Supplied PDFs are retained as mechanical/package references but conflict with the physical chip identity

## Source Conflicts
- Supplied PDF body and schematic U1 identify VL53L4CD, while filenames identify VL53L0X
- Physical module rejects the VL53L4CD 16-bit register protocol; using it produced `0x010F -> 0x0F01`
- Physical module responds to the VL53L0X 8-bit protocol with `0xC0 -> 0xEE` and completes continuous ranging
- Recommendation: for this module batch, use physical model-ID/ranging validation and the VL53L0X datasheet/API as authority; treat the supplied VL53L4CD PDFs as mismatched documentation

## Code Implementation Guide

### Operating Model
- Module behavior: active time-of-flight distance sensor operated through I2C;
  the driver configures the ranging engine, waits for measurement readiness and
  returns a validated distance in millimetres.
- Mental model: model identification and sensor setup precede ranging; each
  report comes from a completed measurement, not from blindly reading one raw
  register at a fixed delay.
- Initialization prerequisite: stable 3V3, 400 kHz-compatible I2C and the
  VL53L0X 8-bit register protocol confirmed on the physical module.

### Initialization Sequence
1. Initialize the I2C transport and live-probe 7-bit address `0x29`.
2. Read model register `0xC0` and require `0xEE`; do not use the mismatched
   VL53L4CD 16-bit-register interpretation.
3. Apply the confirmed VL53L0X initialization/tuning/calibration order from ST
   documentation or the verified current driver, with bounded error handling.
4. Configure/start the selected ranging mode, clear stale interrupt/status and
   mark the instance ready only after setup succeeds.
- Order constraints: model ID precedes configuration; initialization ordering
  must remain intact; readiness/status is checked before consuming a result.

### Runtime Data or Command Path
```text
scheduled sample -> start/continue ranging -> wait data ready with timeout ->
read range/status -> millimetre result -> validity/out-of-range classification ->
latest result -> 5 Hz UART0 presentation
```
- Blocking points: bounded ready polling and I2C transfer, each with no more
  than three attempts where the operation is safely retryable.
- Completion evidence: a valid model/range transaction proves sensor protocol
  operation; actual ranging performance still depends on target and environment.

### Data Conversion and Validity
- Raw format: VL53L0X multi-byte register values are big-endian; public output is
  the calculated/reported millimetre distance, not raw register bytes.
- Conversion: use ST's confirmed range-result semantics rather than inventing a
  linear formula from register bytes.
- Validity checks: communication result, data-ready timeout, sensor range status
  and the project nominal 2000 mm boundary.
- Invalid/out-of-range behavior: above 2000 mm returns the module-specific
  out-of-range result; timeout and communication failures remain distinct.

### Timing and Scheduling Characteristics
- Power/config delays: follow the verified VL53L0X initialization sequence;
  exact internal tuning delays remain owned by that sequence.
- Measurement/update timing: target presentation is 5 Hz, one result every
  200 ms. The ready wait is bounded so other tasks continue.
- Recommended effective rate: 5 Hz for the specified example; a faster rate may
  be used only if the selected ranging timing budget and transport support it.
- Periodic schedule or minimum action gap: measurement/reporting is periodic;
  missed samples are not replayed as bursts.
- Work that can proceed while waiting: other FreeRTOS tasks run between bounded
  polls/transactions.

### Lifecycle and Recovery
```text
uninitialized -> probing -> identified -> configured -> online -> ranging
```
- Transient failure: retry the safe complete transaction up to three times.
- Out-of-range sample: device remains online; only the measurement validity is
  out of range.
- Consecutive communication failure: mark this instance offline and locally
  live-probe/reinitialize it without affecting other modules.
- Identity mismatch: initialization fails immediately and does not attempt the
  wrong chip's setup sequence.

### Direct Implementation Notes
- BSP/API boundary: current `bsp_i2c`, per-device handle at `0x29`, no ESP-IDF
  driver types in the public module header.
- Instance/buffer model: static/fixed pool with transport-neutral distance/status
  result and no periodic allocation.
- Polling/callback/task consideration: bounded polling is sufficient because
  GPIO1 is optional and the current example uses the four-wire I2C connection.
- Public API should expose distance, validity/range status and module errors.

### CH32 Bridge Implementation Notes
- Required generic gateway: `ch32_i2c_multi_gateway_final` only.
- Stable-node and live-presence validation: validate F2-confirmed stable node and
  live-probe `0x29`; do not rely solely on an address snapshot.
- Transfer expansion: preserve VL53L0X 8-bit register addressing and use the
  gateway's chunked read completion for multi-byte results.
- Timeout/idempotency: bounded whole-read retries; configuration writes resume
  only from a known initialization step.
- Existing generic capability assessment: sufficient and validated by the
  current bridge driver/example.
- Discovery/business boundary: discovery manages node assignment; driver owns
  sensor protocol; application owns thresholds and combined alert behavior.

### Module-Specific Difficulties and Common Mistakes
- Distinguishing characteristic: supplied documents describe VL53L4CD, while the
  physical module has been conclusively identified and ranged as VL53L0X.
- Likely mistake: using 16-bit VL53L4CD registers against this device; require
  `0xC0 -> 0xEE` before configuration.
- Likely mistake: treating out-of-range as transport failure; keep measurement
  validity separate from device online state.
- Likely mistake: indefinite ready polling; use a bounded timeout and let other
  modules continue.
- Hardware validation focus: near/far targets, >2000 mm classification, ambient
  light/reflectance effects, disconnect recovery and stable 5 Hz reporting.

### Implementation Complexity
- Direct implementation: medium.
- CH32 bridge implementation: medium.
- Main complexity sources: correct ST initialization order, data-ready/status
  interpretation and source-document identity conflict.
- Reusable existing capabilities: physically verified direct driver, current
  bridge driver, `bsp_i2c` and CH32 I2C gateway.
- Possible generic capability gaps: none currently confirmed.

### Guide Evidence Classification
- Confirmed from user material/physical validation: actual VL53L0X identity,
  `0x29`, `0xC0 -> 0xEE`, 3V3, successful ranging and 5 Hz requirement.
- Supplemented from official sources: ST VL53L0X datasheet/API initialization,
  result and range-status semantics listed in Authoritative Sources.
- Inference: decoupled latest-result publication and local recovery follow the
  module behavior and current project rules.
- Pending hardware verification: production offset/crosstalk calibration and
  usable range across target reflectance and ambient-light conditions.

## Module-Specific Error Codes
- `ERR_VL53L0X_OUT_OF_RANGE` (-10)
- `ERR_VL53L0X_ID_MISMATCH` (-11)
- `ERR_VL53L0X_DATA_NOT_READY` (-12)
- `ERR_VL53L0X_COMM` (-13)

## Changelog
- 2026-08-12: physical validation corrected identity from documented VL53L4CD to actual VL53L0X; direct driver compiled, flashed and ranged successfully
- 2026-08-12: Added the reusable Code Implementation Guide while preserving the physical VL53L0X identity as authority over mismatched supplied documents
