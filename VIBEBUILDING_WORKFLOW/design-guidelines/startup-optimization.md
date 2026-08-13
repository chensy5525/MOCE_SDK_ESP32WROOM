# Startup Time Optimization Strategies

## SECTION 1: Problem Background

- ESP32 time from power-up to operational state consists of multiple serial phases: local scan -> wait CH32 boot -> CH32 discovery & ID assignment -> device init -> identity confirmation.
- Currently, each phase uses long fixed timeouts for robustness. As module count and gateway types grow, cumulative wait time can increase significantly.
- Optimization goal: compress "power-up to system usable" time without sacrificing robustness.

## SECTION 2: Strategy 1 -- Parallel Discovery Within Type, Independent Across Types

- The three gateway types (I2C/SPI/UART) share CAN ID `0x000` but are independent at the frame level via device_type, and their allocators are logically separate.
- As long as CAN transaction mutex granularity allows, I2C and SPI discovery/init can run in parallel with UART listening discovery, reducing N serial phases to the duration of the longest single phase.
- Constraint to watch: if UART uplink is bidirectional and the remote device may
  push large amounts of data right after power-up, and the composition declares
  I2C/SPI initialization as an explicit startup prerequisite, defer opening the
  UART uplink until that prerequisite is complete.  This is a dependency and
  bandwidth decision, not a module-importance or task-priority decision.

## SECTION 3: Strategy 2 -- Adaptive Discovery Timeout

- Currently uses fixed timeout waiting for CH32 reports, set to worst-case coverage. Even if all CH32s report within 1 second, the system waits until timeout.
- Adaptive approach: if no new token appears for a continuous period AND discovered CH32 count meets expectation (when expectation config exists), exit the current discovery window early.
- Without an expected count reference: set a shorter "silence threshold" -- exit early if no new F0 appears for N ms within the discovery window.
- In normal operation this saves significant time; only degrades to original fixed timeout when a CH32 is unusually slow to boot.

## SECTION 4: Strategy 3 -- Overlap Hard Waits with Useful Work

- Currently there is a fixed wait after local I2C scan to allow CH32 gateways enough time to power up and initialize their CAN transceivers.
- Local I2C scan itself is very fast. Reorder: perform local device work DURING the CH32 wait window, or complete the wait first then scan (local devices don't care whether CH32 is ready). Either way reduces pure-wait time.
- Most efficient: move all non-dependent operations between "wait CH32 ready" and "CH32 discovery" into the wait window for parallel execution.

## SECTION 5: Strategy 4 -- Cross-Node Device Init Interleaving

- Currently device init is serial: configure sensor A behind CH32#1 -> wait ACK -> configure sensor B -> wait ACK -> then process CH32#2.
- Transactions to different CH32 nodes (I2C/SPI) have no dependencies on each other. Send config command to CH32#1, do not wait for ACK, immediately send config command to CH32#2, then collect all ACKs together.
- Bottleneck: CAN transaction mutex granularity. If the mutex wraps "send + wait ACK" as one atomic operation, interleaving is impossible. Solution: split send and receive-ACK into separate critical sections, or use async callback model.
- Multiple devices behind the same CH32 (same I2C bus, different addresses): still recommend serial config, since the CH32 itself processes downstream requests single-threaded.

## SECTION 6: Strategy 5 -- Phase-Differentiated Timeout Parameters

- The same operation has different timeout needs in different phases. Example reading sensor data: startup phase only cares "does device respond" -- use shorter timeout for quick presence check; runtime phase must handle comm jitter and retries -- use longer timeout for data reliability.
- Implementation: define a "fast-probe" parameter set for startup-phase device detection and init config (shorter timeouts, fewer retries), separate from normal runtime parameters.
- Devices that fail fast-probe during startup are not immediately abandoned; a background task can retry with full timeout params after entering the main loop.

## SECTION 7: Strategy 6 -- Progressive Startup

- Currently all device discovery, init, and identity confirmation must complete before entering the main loop. User-perceived "power-on to usable" time equals the sum of all phases.
- Progressive startup: register local direct-connect devices first -> immediately enter main loop and start working -> CH32 devices discovered asynchronously, init one by one, dynamically join device table.
- Effect: user may see local sensor data within ~1 second, with CH32 devices gradually joining in the background. Total startup completion time unchanged, but "time to first usable" is drastically reduced.
- Must handle: device table growing dynamically at runtime. Business logic (display, alerts) must tolerate the intermediate state of "device table partially ready".
- CH32 gateways themselves can appear as "devices" in the device table -- only scan downstream after a CH32 is ready, rather than waiting for all CH32s.

## SECTION 8: Strategy 7 -- Adaptive Runtime Maintenance

Startup optimization must continue into normal operation rather than replacing
one long startup wait with frequent disruptive rediscovery.

- Discover immediately at power-up, then maintain incrementally in a FreeRTOS
  background task.
- While any required composition role is missing/offline, use a 10-second
  rediscovery interval.  Once all required roles are online, use 30 seconds.
- Role completeness is explicit composition state, not a raw node-count test.
- Rediscovery merges stable records in place and locally initializes only a new
  or recovering role.  It does not restart healthy drivers or the CAN core.
- Bound each discovery/probe/init operation and release shared transport locks
  between unrelated operations so runtime sampling and outputs continue.
- Do not use different application task priorities as an optimization shortcut
  unless an explicit real-time constraint requires them.  Prefer bounded waits,
  coalesced state, short lock scope and removal of duplicate traffic.
