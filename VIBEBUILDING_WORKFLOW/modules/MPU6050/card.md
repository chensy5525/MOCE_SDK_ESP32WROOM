# MPU-6050 Six-Axis IMU Module Card

## Hardware Model
InvenSense MPU-6050, three-axis gyroscope + three-axis accelerometer, 24-pin QFN; the supplied module schematic identifies U1 as `MPU-6050`

## Module Type
sensor-data

## Communication Interface
I2C, direct / bridged

## Comm Parameters

### I2C
- Device address: 0x68 (7-bit, fixed on this module because AD0 is tied to GND)
- Chip-supported alternative address: 0x69 (7-bit when AD0 is high; not selectable on the supplied module)
- Max speed: 400 kHz
- Register addressing: 8-bit register address followed by single-byte or auto-incrementing burst data
- Project bus setting: 400 kHz

## Pin List
- Direct ESP32: `PIN_MPU6050_SDA = GPIO_21`, `PIN_MPU6050_SCL = GPIO_22`
- Module power: 3V3
- Module ground: GND; share ground with the ESP32 or CH32 bridge
- U2 WAFER-GH1.25-4PWB schematic pinout: pin 1 = 3V3, pin 2 = SCL, pin 3 = SDA, pin 4 = GND
- Alternate H2 2.54 mm 1x4 footprint drawing: pin 1 = 3V3, pin 2 = GND, pin 3 = SCL, pin 4 = SDA; population status is [待确认]
- INT: present on MPU-6050 pin 12 but not routed to the supplied four-wire module connector
- AD0: tied to GND on the module; not exposed

## Hardware Notes
- Power-up stabilization time: wait for register access readiness [待确认]; gyroscope zero-rate output settling is typically 30 ms to within 1 deg/s of final value when `DLPFCFG=0`
- Enable/reset pin: none exposed; wake/reset is register-controlled, with register address and bit definitions [待确认]
- Supply: module must be powered from 3V3 as specified by the user; chip VDD range is 2.375-3.46 V and the schematic connects both VDD and VLOGIC to 3V3
- Typical chip current: 3.8 mA with gyroscope + accelerometer enabled and DMP disabled; 3.9 mA with gyroscope + accelerometer + DMP enabled
- On-board bus pull-ups: SDA and SCL each use 4.7 kOhm to 3V3
- Gyroscope: 16-bit output, selectable full scale +/-250, +/-500, +/-1000, or +/-2000 deg/s; corresponding scale factors 131, 65.5, 32.8, or 16.4 LSB/(deg/s)
- Accelerometer: 16-bit two's-complement output, selectable full scale +/-2, +/-4, +/-8, or +/-16 g; corresponding scale factors 16384, 8192, 4096, or 2048 LSB/g
- Project full-scale selections: [待确认]
- Project digital low-pass filter and internal sample rate: [待确认]
- Calibration: gyroscope zero-rate bias and accelerometer offsets must be determined at startup or supplied from stored calibration; procedure and stationary interval are [待确认]
- Independent power supply: no; use 3V3 and common ground
- Protection: no board-level over-voltage, reverse-polarity, over-current, short-circuit, or ESD protection is confirmed by the supplied schematic
- Known limitation: a six-axis accelerometer/gyroscope can gravity-reference roll and pitch, but it has no absolute heading reference. Yaw obtained by integrating Z-axis angular rate is relative and drifts unless an external heading reference or another stated correction method is used.

## Measurement and Output Contract
- Direct target phenomenon: ESP32 UART0 prints the latest calculated orientation at 5 Hz (one line every 200 ms), containing three angle values only as the primary measurement fields; do not print raw register values as the target output
- Bridge target phenomenon: identical to direct mode - ESP32 UART0 prints the latest calculated orientation at 5 Hz (one line every 200 ms), containing three angle values only as the primary measurement fields; the CH32 performs transparent I2C forwarding and no orientation calculation
- Output fields and units: `roll_deg`, `pitch_deg`, `yaw_deg`, in degrees
- Angle coordinate frame, positive directions, zero pose, wrap ranges, and Euler rotation order: [待确认]
- Orientation calculation method: [待确认]
- Yaw semantics/correction source: [待确认]
- Calculation rate may be higher than the 5 Hz print rate; the calculation rate and timestamp/delta-time source are [待确认]
- Validity rule: do not publish a newly calculated sample after an I2C read failure; retain failure counters/state separately and let the main loop continue

## Identification Method
- Candidate identification: I2C ACK at fixed 7-bit address 0x68
- Register identification: official `WHO_AM_I` register address, mask, and expected value are [待确认] because the supplied product specification refers to a separate MPU-6000/MPU-6050 Register Map and Register Descriptions document that was not supplied

## Bridge Constraints
- Required constraints: use `ch32_i2c_multi_gateway_final`; the stable CH32 node reference comes from the discovery layer, and the module driver must not discover or assign node IDs. Use a non-zero request ID and collect `STATUS_READ_CHUNK` plus `STATUS_READ_DONE` for multi-byte reads. Acquire one coherent accelerometer/temperature/gyroscope burst per sample; the first data-register address and burst length are [待确认]. ESP32 and CH32 use the current shared CAN-core routing and transaction lock. CH32 downstream I2C must operate at no more than 400 kHz.
- Bridge timeout: [待确认]
- Retry/idempotency: register reads are retryable as whole transactions after timeout; configuration writes must be checked and retried only from a known initialization step
- Recommended implementation: compute and calibrate orientation on ESP32 for both direct and bridge modes, use the same transport-neutral measurement structure and fusion implementation in both packages, keep calculation sampling separate from the 5 Hz UART0 presentation rate, and avoid DMP use unless the missing official register/DMP material is supplied and explicitly selected

## Dependencies
- Depends on: none
- Typically used with: ESP32, CH32V203 I2C bridge (bridge mode)

## Software Package Paths
- Direct driver: `components_direct/mpu6050_direct/`
- Direct example: `examples_direct/mpu6050_direct_test/`
- Bridge driver: `components_ch32/ch32_mpu6050_gateway/`
- Bridge example: `examples_ch32/mpu6050_ch32_test/`

## Transport Dependencies
- Direct BSP: `bsp_i2c`
- Bridge protocol layer: `ch32_i2c_multi_gateway_final`
- Shared CAN core: `ch32_can_gateway_core` (transitive only; never called by the module driver)
- Required CH32 firmware: [待确认]

## Identification Confidence
- WHO_AM_I available: yes at chip level, but its authoritative register address/mask/expected value are [待确认] pending the official register-map document
- Address-only identification: yes; 0x68 is a strong candidate clue but is not unique proof of MPU-6050
- Protocol confirmation: [待确认]
- Manual configuration required: yes until register-based identification is confirmed

## Authoritative Sources
- Chip product specification: `C:/Users/chenqing/Desktop/第一批模块资料/MPU6050/chip datasheet_MPU6050 six-axis IMU module.pdf` - authoritative for chip model, electrical limits, supported interface, ranges, scale factors, timing, and chip-level capabilities
- Module schematic: `C:/Users/chenqing/Desktop/第一批模块资料/MPU6050/SCH_MPU6050 six-axis IMU module.pdf` - authoritative for module supply, fixed AD0 state/address, pull-ups, connector nets, and exposed signals
- Register map: [待确认] - the supplied chip product specification explicitly delegates register addresses and bit definitions to a separate document
- CH32 firmware: [待确认]
- Verified ESP32 example: [待确认]
- Hardware validation date/result: [待确认]

## Source Conflicts and Resolution
- No conflict was found between the chip product specification and the main U1/U2 electrical schematic: both support MPU-6050, 3V3 operation, I2C, and address 0x68 with AD0 low.
- The supplied schematic page shows two different four-pin connector definitions: connected U2 is `3V3/SCL/SDA/GND` on pins 1/2/3/4, while the alternate H2 footprint drawing is `3V3/GND/SCL/SDA` on pins 1/2/3/4. Treat them as connector-specific pinouts rather than interchangeable wiring. Use the populated connector's silkscreen and continuity measurement as the final authority; population status is [待确认].
- If register values found in examples or historical context differ from the missing official Register Map, use the official InvenSense MPU-6000/MPU-6050 Register Map and Register Descriptions revision matching this MPU-6050 as the authority.

## Code Implementation Guide

### Operating Model
- Module behavior: continuous sampled sensor. The accelerometer and gyroscope
  produce signed three-axis samples; the requested roll, pitch and yaw are
  calculated values rather than directly readable angle registers.
- Mental model: one coherent six-axis burst is timestamped, converted to `g`
  and `deg/s`, corrected by calibration, then passed to the selected orientation
  estimator. UART0 presentation at 5 Hz is separate from the estimator update
  cadence.
- Initialization prerequisite: the module must be stationary for the selected
  startup bias-calibration procedure. The exact calibration duration and fusion
  algorithm remain [待确认].

### Initialization Sequence
1. Power from 3V3, wait for register access readiness, and initialize the I2C
   transport at no more than 400 kHz.
2. Live-probe address `0x68`, then read the official `WHO_AM_I` signature before
   writing configuration. The register-map values are still [待确认].
3. Wake/reset the chip through the confirmed power-management registers, choose
   accelerometer/gyroscope ranges, DLPF and sample rate, then read back critical
   configuration. Those register definitions must come from the missing
   official register-map document.
4. Collect stationary samples to estimate offsets, initialize estimator state
   and timestamp, then mark the instance ready.
- Order constraints: identity precedes configuration; range selection precedes
  scale conversion; calibration precedes publishing orientation.

### Runtime Data or Command Path
```text
scheduled sample -> coherent accel/temp/gyro burst -> signed conversion ->
bias correction -> physical units -> orientation update -> validity check ->
latest roll/pitch/yaw snapshot -> 5 Hz UART0 presentation
```
- Blocking points: bounded I2C burst and finite transport retries. Never wait
  indefinitely for a sample or retry one failure forever.
- Completion evidence: a successful I2C burst proves transport data delivery;
  it does not prove calibration quality or absolute yaw accuracy.

### Data Conversion and Validity
- Raw format: big-endian, signed 16-bit two's-complement values for each axis.
- Conversion: divide raw acceleration and angular-rate values by the scale
  factor selected during init; apply measured offsets before fusion.
- Validity checks: reject samples after transport failure, impossible length,
  invalid configured scale or estimator numeric failure.
- Invalid/out-of-range behavior: do not publish a new angle sample; retain
  device state and failure counters so the next scheduled sample can recover.
- Yaw is relative integrated heading and drifts without an external reference;
  it must not be described as absolute compass heading.

### Timing and Scheduling Characteristics
- Power/config delays: register-readiness delay and exact reset/wake waits are
  [待确认]; the product specification notes typical gyro settling near 30 ms for
  the stated condition.
- Measurement/update timing: estimator cadence should be higher than or equal
  to the requested 5 Hz presentation, with measured delta time for integration.
- Recommended effective rate: UART0 output is fixed at 5 Hz by the requirement;
  internal sample/fusion rate is [待确认] and must match the chosen DLPF/sample
  configuration.
- Periodic schedule or minimum action gap: sampling is periodic; presentation is
  periodic. Neither should replay stale missed samples.
- Work that can proceed while waiting: other tasks may run between scheduled
  samples; calibration should be isolated from unrelated module operation.

### Lifecycle and Recovery
```text
uninitialized -> identifying -> configuring -> calibrating -> online -> sampling
```
- Transient failure: retry a complete read transaction up to the documented
  bound, then report the sample invalid.
- Invalid sample: affects only that sample and does not immediately make the
  module offline.
- Consecutive communication failure: mark this instance offline and let the
  owning example/device manager live-probe and locally reinitialize it.
- Identity or calibration failure: initialization fails without publishing
  angles; other modules continue running.

### Direct Implementation Notes
- BSP/API boundary: use the current `bsp_i2c` public API and a per-device handle;
  do not expose ESP-IDF I2C types in the MPU6050 public header.
- Instance/buffer model: static instance or fixed pool; one fixed burst buffer
  and transport-neutral orientation result.
- Polling/task consideration: use a periodic sampling context because delta time
  affects integration. The unavailable INT pin prevents interrupt-driven data
  ready on the supplied four-wire connector.
- Public API should expose calculated angles and validity/status, not raw
  register values as the primary result.

### CH32 Bridge Implementation Notes
- Required generic gateway: `ch32_i2c_multi_gateway_final` only.
- Stable-node and live-presence validation: validate F2-confirmed stable node,
  set the downstream speed and live-probe `0x68`; do not trust only a discovery
  address snapshot.
- Transfer expansion: acquire the coherent multi-byte sample through the
  gateway chunked-read API with a non-zero request ID and READ_DONE completion.
- Timeout/idempotency: reads can retry as whole transactions; configuration
  writes restart only from a known initialization step.
- Existing generic capability assessment: multi-byte read/write capability is
  recorded as available; exact required register/burst definitions and CH32
  firmware path remain [待确认].
- Discovery/business boundary: the driver consumes the stable node and performs
  orientation semantics on ESP32; it never owns discovery or CAN.

### Module-Specific Difficulties and Common Mistakes
- Distinguishing characteristic: six-axis samples do not equal three absolute
  angles. Roll/pitch can use gravity correction, while yaw lacks an absolute
  reference and accumulates drift.
- Likely mistake: calculating at the 5 Hz print rate with a fixed delta time can
  produce poor dynamic response; use a measured estimator cadence and decouple
  presentation.
- Likely mistake: applying scale factors that do not match configured ranges;
  bind conversion constants to the configuration selected during init.
- Datasheet/revision ambiguity: register addresses and power-management bits
  must not be copied from an unverified clone or historical example.
- Hardware validation focus: stationary bias, sign/axis convention, zero pose,
  dynamic roll/pitch response and expected yaw drift.

### Implementation Complexity
- Direct implementation: high.
- CH32 bridge implementation: high.
- Main complexity sources: missing confirmed register map, coherent burst read,
  calibration, estimator selection, timestamped integration and yaw semantics.
- Reusable existing capabilities: current I2C BSP, CH32 I2C chunked reads,
  static-instance/error/logging conventions.
- Possible generic capability gaps: none confirmed; exact burst and timing needs
  cannot be finalized before the register map and estimator choices are confirmed.

### Guide Evidence Classification
- Confirmed from user material: MPU-6050 model, 3V3 module wiring, fixed `0x68`,
  scale-factor options, no exposed INT/AD0 and 5 Hz three-angle output goal.
- Supplemented from official sources: chip electrical/range/settling facts in
  the supplied InvenSense product specification.
- Inference: decoupled estimator/presentation structure and local recovery are
  implementation conclusions derived from the required angles and project rules.
- Pending hardware verification: official identity/config registers, selected
  ranges/DLPF/sample rate, calibration interval, coordinate convention, fusion
  method and yaw acceptance criteria.

## Module-Specific Error Codes
- `ERR_MPU6050_ID_MISMATCH` ([待确认]): device ACKed at 0x68 but register identification did not match the confirmed MPU-6050 identity signature
- `ERR_MPU6050_CALIBRATION` ([待确认]): startup calibration could not obtain a valid stationary bias estimate
- `ERR_MPU6050_DATA_INVALID` ([待确认]): acquired or calculated orientation sample failed validity checks

## Changelog
- 2026-08-12: Initial module card created from the supplied MPU-6050 product specification, module schematic, project global rules, module card template, board classification, code-generation rules, and user-provided direct/bridge output requirements
- 2026-08-12: Added the reusable Code Implementation Guide while preserving unresolved register-map, calibration, fusion and coordinate fields as pending confirmation
