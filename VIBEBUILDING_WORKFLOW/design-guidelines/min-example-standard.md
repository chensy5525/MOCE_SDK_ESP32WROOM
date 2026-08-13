# Minimum Example Output Standard

## SECTION 1: What is a Minimum Example

- A minimum example is the simplest code that verifies a single module's basic function. It contains only the module itself + ESP32 main controller. No dependency on other modules.
- Purpose: quickly verify "wiring is correct, module can be identified, basic function works".
- Every module must pass its minimum example before ingest. Output must conform to this standard.

## SECTION 2: Unified Verification Output

- All modules' minimum example output uses one of two forms (determined by module type):
  - **Serial print**: debug UART (UART0) outputs structured data and status. This is supported by all modules.
  - **OLED display**: if the system has an OLED connected, the minimum example MAY also display key data on OLED as a secondary observation aid. NOT mandatory.
- "Code compiles" or "saw an LED blink" is NOT an acceptable standard. Output must be readable and comparable.

## SECTION 3: Per-Type Minimum Example Output

- **Sensor-data**: after init OK, periodically print physical quantity. Example: `[INF][VL53L0X] dist=1234mm`. Cover: normal values, boundary values, out-of-range.
- **Sensor-event**: after init OK, wait for events. Print on event. Example: `[INF][PIR] motion detected`. Cover: event present, no event, consecutive events.
- **Actuator**: after init OK, execute a fixed action sequence and print state. Example: `[INF][RELAY] ch1 ON` -> delay -> `[INF][RELAY] ch1 OFF`. No continuous high-frequency switching required.
- **Driver**: after init OK, small-amplitude low-speed motion and print feedback. Example: `[INF][MOTOR] duty=10% rpm=120`. Verify no-load first, then loaded. Estop function must be separately triggered and output confirmed off.
- **Communication**: after init OK, periodically print connection state. Send test data and print peer response. Example: `[INF][HM10] connected`, `[INF][HM10] rx: hello`.
- **HMI-output**: after init, display fixed test content. Example: OLED pages showing module name, comm params, current state. Must visually confirm content is correct, no garbled text.
- **HMI-input**: after init, periodically print input state. Example: `[INF][KEY] key1 pressed`.

## SECTION 4: Minimum Log Requirements

- Every module must output the following log points (in order):
  1. Init log: `[INF][TAG] init OK, <key params>` or `[ERR][TAG] init FAIL, reason=xxx`
  2. Periodic function log: at least 5 consecutive cycles to prove it's not a one-off success.
  3. Deinit log: `[INF][TAG] deinit OK`
- If the module supports anomaly detection (e.g. disconnect), additionally output: `[WRN][TAG] xxx` or `[ERR][TAG] xxx`.

## SECTION 5: What a Minimum Example Must NOT Do

- Must NOT depend on other modules to run (main controller excepted). A sensor minimum example must not require OLED to be initialized first.
- Must NOT skip error handling. Even in a minimum example, comm failure must print error logs. Do not assume hardware is always perfect.
- Must NOT wait indefinitely. Read operations must have timeout. On timeout, print log and continue.
- Must NOT contain business logic (e.g. "alert if distance < 100mm"). Business logic belongs in composition examples, not single-module minimum examples.

## SECTION 6: Minimum Example File Organization

Each minimum example is a complete ESP-IDF project, not a loose `.c` file:

```
examples_direct/<module>_direct_test/
examples_ch32/<module>_ch32_test/
```

Each project contains at least `CMakeLists.txt`, `main/CMakeLists.txt`,
`main/main.c` and `README.md`.

### Direct minimum example

- Contains one module driver and the BSP initialization it requires.
- Does not require a device table or CH32 discovery.
- May use FreeRTOS delay primitives required by ESP-IDF/module timing, but does
  not add unrelated background services.

### CH32 bridge minimum example

- Contains one kind of downstream business module, while retaining the real
  shared CAN infrastructure required by the bridge architecture.
- Must use F0/F1/F2 dynamic discovery and a stable CH32 node record.
- May use the shared CAN receive task.  If runtime rediscovery is retained, it
  follows the adaptive role policy: 10 seconds while the module role is missing
  and 30 seconds while it is online.
- Must never hardcode node_id merely to make the example shorter.

## SECTION 7: Multi-Module Composition Example Standard

A composition example is not a minimum example for any one module.  It is a
reusable reference showing how verified independent drivers are combined.

- Its primary design goal is decoupling and transferability to later
  AI-generated code; producing the requested visible phenomenon is necessary
  but not sufficient.
- Reuse verified concrete module drivers unchanged where possible.  Drivers do
  not contain composition thresholds, display text, speech policy or discovery
  ownership.  The application layer owns those relationships.
- Multi-module examples use FreeRTOS by default.  AI selects task boundaries
  from cadence, blocking behavior, shared resources and recovery isolation.
- Application tasks use one common priority unless an explicit real-time
  requirement states otherwise.  Do not encode subjective module importance
  through task priorities.
- Use producer/consumer state flow: acquisition publishes a bounded latest
  state; display, speech and other consumers read it independently.  A slow
  consumer must not delay acquisition or another consumer.
- Discovery maintenance is separate and adaptive: immediate at startup,
  10-second incremental intervals while any required role is missing, and
  30-second intervals while every required role is online.
- Each role has an independent lifecycle and recovery path.  One failed role
  does not restart ESP32, the CAN core or healthy modules.
- Bulk outputs use change detection and bounded latest-state buffering.  OLED
  examples use dirty pages/regions and the highest stable effective refresh
  rate supported by the real transport budget.
- Periodic sampling and minimum side-effect gaps use different timing logic;
  missed speech/alarm periods are not replayed back-to-back.
- README documents task grouping rationale, periods, required roles, recovery
  behavior and pending hardware validation without calling one role more
  important than another.
- Startup logs identify reset reason and configured rediscovery/output periods.
  Logs clearly distinguish boot, rediscovery, rebind and module recovery.
