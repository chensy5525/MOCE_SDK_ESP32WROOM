# Actuator Driver Guidelines

## SECTION 1: General Flow

- init: configure comm interface -> verify device presence (e.g. I2C ACK) -> set output to safe state -> configure initial state -> mark initialized
- deinit: set output to safe state FIRST, then release bus resources. Order must not be reversed -- releasing resources first may leave outputs floating in an undefined state.
- write: receive command -> validate (channel number, parameter range) -> execute action -> optional feedback confirmation
- read: return current output state or fault status. Return `ERR_NOT_SUPPORTED` if not available.
- reset: restore output to safe state without releasing resources.

## SECTION 2: Safe State Principle

- "Safe state" is determined by the module itself; documented in card.md. Example: relay defaults to open (normally open), solenoid defaults to closed.
- init and deinit must use the same safe state, ensuring the module is in the same state at both ends of its lifecycle.
- On unexpected power loss or comm failure, hardware should auto-return to safe state. If the hardware lacks this capability, clearly note it in card.md.

## SECTION 3: Action Execution

- Must check enable conditions before acting. Return module-specific error code if conditions not met.
- Multi-channel actuators (e.g. multi-relay) must carry channel number in write().
- Time-windowed actions (e.g. pulse output for N milliseconds): timeout mechanism documented in card.md.
- Recommended log after action: `[INF][RELAY] ch1 ON`

## SECTION 4: Interface-Specific Notes

- **I2C**: Same as sensor guidelines. Multi-channel actuators typically distinguish channels via register address or data bits. No address conflicts on same I2C bus.
- **UART**: Same as sensor guidelines. Actuator may return confirmation frame or fault code; handle reception. Protocol frame format and device identification documented in card.md.
- **GPIO**: Direct connection to ESP32 pins, controlled via high/low level. Mind drive capability -- relay coils etc. cannot be driven directly by GPIO; must go through transistor or driver chip. Opto-isolated actuators: mind common ground.
- **PWM**: Servos and similar PWM-controlled actuators: frequency and duty cycle range documented in card.md. Multiple servos may need multiple timer channels; mind hardware resource allocation.

## SECTION 5: General Notes

- High-power actuators must use independent power supply, with common ground to main controller. Power-up sequence (control power first or motor power first) documented in card.md.
- Whether post-action delay is needed (e.g. relay pull-in stabilization time): delay value in card.md.
- No action execution inside ISR. All actions performed in main loop or tasks.

## SECTION 6: Direct vs. Bridged Versions

Direct and bridge versions are independent software packages:
- `<abbr>_direct.c` — direct ESP32 connection
- `<abbr>_bridge.c` — CH32 CAN bridge connection

Keep operation semantics aligned where practical, but do not force direct and
bridge cfg types or public headers to be identical.

**Key differences in bridge version:**
- The bridge package calls only the verified protocol component for its actual
  downstream interface; it never constructs CAN frames directly.
- Handle holds a stable CH32 node reference and `bridge_timeout_ms`; it does not cache a copied runtime node_id
- GPIO control is available only if the corresponding CH32 firmware and ESP32
  protocol layer implement and verify it.  Do not invent `can_send_gpio_*` APIs.
- `_init` receives the stable node reference from device discovery and validates F2-confirmed `ready`, token, and node_id range
- Safe state sequence unchanged — `_set_all_safe()` still called before marking initialized, but underlying `_set_channel()` sends CAN frames instead of direct GPIO

**Templates:**
- `_templates/actuator_direct.c`
- `_templates/actuator_bridge.c`
