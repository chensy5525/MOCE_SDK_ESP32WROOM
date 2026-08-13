# Logging System

## SECTION 1: Log Levels

- `LOG_DBG` (0)  Debug detail. Can be globally disabled in release builds.
- `LOG_INF` (1)  Key state changes (init OK, connection established, device identified)
- `LOG_WRN` (2)  Recoverable anomalies (timeout retry, checksum fail & resend, temporary no-response)
- `LOG_ERR` (3)  Unrecoverable errors (no hardware response, init failure, ID mismatch)

## SECTION 2: Output Format

- Fixed format: `[LEVEL][MODULE_TAG] message`
- Example: `[INF][BMP280] init OK, addr=0x76`
- Example: `[ERR][RELAY] i2c timeout on ch1`
- MODULE_TAG is the module abbreviation, declared at ingest time, globally unique
- Message text: Chinese or English not enforced, but keep consistent within a single module

## SECTION 3: Output Channel & Multi-Module Convention

- All logs go to UART0 (debug serial). Never log on business UARTs.
- Each module wraps log calls in internal macros that auto-prepend the module tag. Callers never write the tag manually.
- In multi-module scenarios, log tags naturally identify the source module.
- Layer tags are reserved as follows: `CH32_CAN_CORE` for TWAI/queue/bus-off,
  `CH32_I2C` for I2C discovery/transport, `CH32_UART` for UART
  discovery/transport, the module abbreviation for module behavior, and
  `<ABBR>_EXAMPLE` for example orchestration.
- CAN faults, F0/F2 discovery failures, downstream scan/probe failures, module
  initialization failures and runtime data failures use distinct messages.
  Do not collapse them all into `COMM_FAIL`.

## SECTION 4: Multi-Instance Log Tags

- Format: `[LEVEL][MODULE_TAG#N] message`
- Single-instance modules keep using `[LEVEL][MODULE_TAG] message`
- Instance number N assigned by ESP32 device manager at discovery time, incrementing from 0 for same-model modules
- Instance numbers exist only at runtime; they do NOT go into the knowledge base or card.md

## SECTION 5: Multi-Module Log Patterns (Reference)

These are the expected log patterns for ESP32 multi-module systems. AI-generated code must produce logs following these patterns so the human reader can understand system state at a glance.

### 5A: Startup Phase Logs

```
# Discovery summary: how many I2C CH32 nodes found this round
[INF][CH32_I2C] discovery result=OK fresh=3 stable_before=0

# New I2C gateway added: stable token=0xF6C4, got node_id=1
[INF][CH32_I2C] node add token=0xF6C4 node=1 total=1

# Scan downstream I2C bus of node=1: found one device at 0x3C
[INF][CH32_I2C] scan node=1 token=0xF6C4 result=OK count=1 addrs=0x3C

# Device identified at 0x3C as OLED, linked to CH32 node=1
[INF][DEVICE_MGR] add name=OLED_1 kind=OLED link=CH32_I2C node=1 addr=0x3C

# UART CH32 discovery: type=0x04 = UART gateway (not VC02/SYN)
[INF][CH32_UART] discover token=0xF608 type=0x04 fw=1 caps=0x.. seq=.. total=1

# Assigning node=49 to UART token=0xF608, attempt 1
[INF][CH32_UART] assign TX token=0xF608 node=49 attempt=1 result=OK

# F2 confirmation received: UART gateway node=49 established
[INF][CH32_UART] assign confirmed token=0xF608 node=49 result=OK cmd=0x231 status=0x131 hello=0x731

# UART route ready, downstream device still unidentified
[INF][CH32_UART] route node=49 result=READY downstream=UNKNOWN
```

Key distinction: `CH32_UART_ASSIGN_TX result=OK` means the CAN frame was sent. `CH32_UART_ASSIGN result=OK` means the CH32 accepted the assignment (F2 received).

### 5B: Runtime Operation Logs

```
# Sensor read: valid measurement. out_of_range=1 means device online but target out of range
[INF][VL53L0X#1] data valid=1 mm=356 out_of_range=0

# IMU raw data with validity check. valid=0 samples don't participate in danger verdict
[INF][MPU6050#1] data valid=1 ax=120 ay=-80 az=16320 gx=4 gy=-2 gz=1

# System health summary
[INF][APP] data danger=0 voice=1 oled_ready=1 vc02_events=3

# Raw VC02 bytes received from a UART route
[DBG][VC02] raw route=CH32_UART node=49 len=8 data=[...]

# Raw bytes matched a known VC02 command, callback dispatched
[INF][VC02] event command=OLED_REFRESH result=DISPATCHED

# SYN voice frame sent to CH32-UART route, complete ACK received
[INF][SYN6288E] tx route=CH32_UART node=50 result=OK
```

Note: `SYN6288E_TX result=OK` only means the CH32-UART bridge confirmed transmission. It does NOT prove the SYN chip actually produced sound.

### 5C: Rediscovery Phase Logs

```
# Light rediscovery: keep existing I2C nodes, only incremental UART
[INF][DEVICE_MGR] rediscovery begin keep_existing_i2c=1 uart_incremental=1

# I2C rediscovery failed this round, but 3 stable nodes kept
[WRN][CH32_I2C] discovery keep existing stable=3 reason=FAILED_OR_TIMEOUT

# Same physical CH32 (token=0xF6C4), node_id changed from 1 to 2
[INF][CH32_I2C] node update token=0xF6C4 old=1 new=2 stable_ref_kept=1
```

Correct semantics of the last log:
- `device_type + token` decides "is this the same physical gateway" (token alone is sufficient only inside an already type-separated table).
- node_id is only current runtime routing.
- When node_id changes: update the node record. Device table pointers remain intact.
- Do NOT delete and recreate OLED/MPU/TOF instances.

## SECTION 6: Runtime Diagnostics and Log Rate Limits

- A multi-module composition prints the ESP32 reset reason once at application
  startup, together with firmware identity and configured discovery/output
  periods when available.  This distinguishes a real reboot from rediscovery.
- Use `boot`, `rediscovery`, `rebind`, `offline`, `degraded` and `recovery` with
  their literal meanings.  Never label an ordinary incremental discovery as a
  restart or reboot.
- Log lifecycle transitions once when state changes.  Do not repeat an
  identical offline/init/read error on every fast producer cycle.
- Repeated recoverable failures use a per-module rate limit or cooldown.  Error
  counters may continue increasing while duplicate text is suppressed.
- A recovery success log names the module, stable token/node reference and the
  previous state.  Healthy modules do not emit new init logs merely because a
  different module recovered.
- A bridge TX success proves only the documented transport acknowledgement; it
  must not be logged as proof of a physical side effect such as audible speech
  or visible display output.
