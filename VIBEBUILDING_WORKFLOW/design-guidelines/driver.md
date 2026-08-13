# Motor Driver Board Guidelines

## SECTION 1: General Flow

- init: FIRST configure protection parameters (overcurrent threshold, stall detection window) -> THEN enable output -> set output to stop/brake -> mark initialized. Order must not be reversed -- protection must be active before output is enabled.
- deinit: brake/stop output -> disable power stage -> release GPIO/timer resources
- write: receive control command (direction+speed / angle / steps) -> validate -> execute -> report current status
- read: return current state (speed, position, fault code). Return `ERR_NOT_SUPPORTED` if no sensor feedback available.
- reset: emergency stop -> clear fault state -> re-enable

## SECTION 2: Protection Mechanisms (driver-board specific)

- Overcurrent protection: current exceeding threshold must immediately cut output. No reliance on manual response. Threshold and response time in card.md.
- Stall detection: motor running but encoder/speed feedback unchanged beyond configured window -> declare stall -> cut output -> report error.
- Overtemperature protection: if driver chip has temperature sensing, report temperature status in read().
- After protection triggers: enter fault-locked state. Recovery requires explicit reset() call. No auto-retry (prevents repeated overcurrent from destroying hardware).

## SECTION 3: Emergency Stop

- Must implement a dedicated estop interface (e.g. `driver_estop`), separate from normal deinit.
- Estop execution: immediately cut motor power -> log fault -> enter fault-locked state.
- Estop can be triggered by: overcurrent, stall, external estop signal (GPIO interrupt).
- Recovery from estop requires manual confirmation or upper-layer logic explicitly calling reset().

## SECTION 4: Control Modes

- Three common motor control modes. Which are supported is documented in card.md:
  - **PWM speed control**: direction pin + speed PWM. Frequency and duty range in card.md.
  - **Step mode**: step count + direction + speed profile. Step angle and accel/decel params in card.md.
  - **Servo mode**: angle PWM, typically 50Hz, duty cycle maps to angle range.
- A single driver board model may support one or more of these modes. Clearly state in card.md.

## SECTION 5: Interface-Specific Notes

- **GPIO + PWM**: Most common. Direction pin + PWM pin + enable pin + fault feedback pin (optional). PWM uses ESP32 LEDC or MCPWM peripheral. Frequency and resolution must match driver board requirements.
- **I2C / UART**: A few intelligent driver boards have built-in comm interfaces. Follow sensor/actuator guidelines for the corresponding interface.
- Independent power: motor power and logic power separate, common ground with main controller. Power-up sequence documented in card.md.

## SECTION 6: General Notes

- Debug driver boards with no load first (motor disconnected). Only connect motor after verifying PWM output is correct.
- Multiple driver boards sharing one motor power supply: mind total power budget. Power limit in card.md.
- When reversing motor direction, insert dead time (1-5ms) to prevent H-bridge shoot-through.
- All protection thresholds as `#define` grouped at top of file for easy tuning.

## SECTION 7: Direct vs. Bridged Versions

Direct and bridge versions are independent software packages:
- `<abbr>_direct.c` — direct ESP32 connection (GPIO+PWM controlled locally)
- `<abbr>_bridge.c` — generated only after an authoritative CH32 firmware and
  matching ESP32 protocol layer support the required remote operations

**Key differences in bridge version:**
- PWM/GPIO/feedback functions use only real, verified public protocol APIs.
  `can_send_pwm_duty()`, `can_send_gpio_config()` and
  `can_send_gpio_read()` are not assumed to exist and must not be invented.
- If a required generic gateway capability is missing, first report it, then
  implement and verify the CH32 firmware and ESP32 protocol layer before the
  module driver is generated.
- All protection logic (overcurrent threshold, stall detection timeout) remains on ESP32 side — only the physical I/O is delegated to CH32
- `_estop` may update local lock state only after the remote cut-off path and
  its acknowledgement semantics have been verified end-to-end.
- `_init` receives a stable CH32 node reference from device discovery; validates F2-confirmed `ready`, token, and node_id range before proceeding, and never discovers/assigns IDs itself

**Templates:**
- `_templates/driver_direct.c`
- `_templates/driver_bridge.c`
