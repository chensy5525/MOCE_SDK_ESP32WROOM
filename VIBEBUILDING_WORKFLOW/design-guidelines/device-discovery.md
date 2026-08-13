# Device Discovery & Session Management

## SECTION 1: Core Principles

- Direct and CH32-bridged are equal connection methods. Both are scanned at power-up; neither is a fallback for the other.
- Multiple instances of the same peripheral are allowed (e.g. local OLED + multiple CH32-OLEDs coexisting).
- I2C uses address for candidate identification. UART requires protocol evidence or explicit config. SPI uses CS pin + WHO_AM_I register.
- CH32 only converts CAN <-> I2C/UART/SPI transport. It carries zero peripheral business logic.
- Failure of one device affects only its own state; it must not block other devices.
- Rediscovery uses incremental merge; never clear the stable node table or device table.

## SECTION 2: CH32's Three Gateway Types

- I2C gateway (device_type=0x01): CH32 downstream I2C bus. ESP32 reads/writes I2C devices indirectly via CAN frames. node_id allocated starting from 1.
- UART gateway (device_type=0x04): CH32 downstream UART device. CAN frames carry fragmented transport protocol (START/DATA/ACK). node_id starting from 0x31.
- SPI gateway (device_type=0x03): CH32 downstream SPI device. Devices distinguished by CS pin. node_id starting from 0x21.
- All three gateways share CAN ID `0x000` for discovery frames (data[0]=0xF0). ESP32 dispatches to the correct allocator based on `data[1]` device_type.
- node_id space is segmented: I2C=[1, 0x20], SPI=[0x21, 0x30], UART=[0x31, 0x4F]. No overlap. Recommended: use a global node_id allocator for all gateway types.

## SECTION 2.1: Discovery Frame Protocol (F0/F1/F2 Byte Layout)

F0 inventory/request, F1 assignment and F2 confirmation use shared CAN ID `0x000`, DLC=8. This is the protocol implemented by the immutable `examples_final/CH32_UART_gateway_dynamic` reference and shared `common_can_node.h`. ESP32 must validate ID and DLC before parsing.

**F0 — CH32 requests node_id (CH32 -> ESP32):**

| Byte | Field | Description |
|------|-------|-------------|
| data[0] | 0xF0 | Discovery request marker |
| data[1] | device_type | 0x01=I2C, 0x03=SPI, 0x04=UART |
| data[2] | fw_version | CH32 gateway firmware version; do not reinterpret this byte as protocol_version |
| data[3] | capability_flags | CH32 capability bitmap |
| data[4] | request_sequence | Rolling sequence number |
| data[5] | token_lo | 16-bit token from CH32 UID, little-endian |
| data[6] | token_hi | |
| data[7] | current_node | 0x00 if unassigned |

**F1 — ESP32 assigns node_id (ESP32 -> CH32):**

| Byte | Field | Description |
|------|-------|-------------|
| data[0] | 0xF1 | Assignment marker |
| data[1] | node_id | Assigned node_id for I2C and UART dynamic gateways |
| data[2] | 0xAA | Magic byte |
| data[3] | 0x55 | Magic byte |
| data[4] | token_lo | Mirrored from CH32's F0 |
| data[5] | token_hi | |
| data[6] | device_type | 0x01=I2C, 0x04=UART |
| data[7] | protocol_version | Currently 0x01 |

Critical: both current I2C and immutable UART dynamic gateways read node_id from data[1], token from data[4..5], device_type from data[6], and protocol_version from data[7]. Do not preserve the historical UART-data[6] layout in new ESP32 code.

**F2 — CH32 confirms assignment (CH32 -> ESP32):**

CAN ID: `0x000`, matching the immutable UART dynamic-gateway reference. I2C multi-node discovery therefore schedules inventory replies in token-derived slots, and ESP32 repeats inventory queries with changing sequence so a same-slot collision can recover. Changing the F2 CAN ID requires a coordinated, versioned protocol change across every gateway and ESP32 parser; one gateway must not change it independently.

| Byte | Field | Description |
|------|-------|-------------|
| data[0] | 0xF2 | Confirmation marker |
| data[1] | node_id | Assigned node_id echoed back |
| data[2] | 0xAA | Magic byte |
| data[3] | 0x55 | Magic byte |
| data[4] | token_lo | Token echoed back |
| data[5] | token_hi | |
| data[6] | device_type | Echoed |
| data[7] | fw_version | CH32 firmware version |

ESP32 must verify: F2's token matches the pending assignment, and node_id matches what was sent. Only then is the CH32 node considered assigned.

## SECTION 2.1.1: Multi-Gateway Collision Recovery

- Every physical CH32 derives a stable token from its UID. ESP32 logs every new F0 token; two boards reporting the same token is an identity fault and cannot be solved by node allocation alone.
- Unassigned F0 responses use token-derived time slots because they necessarily share CAN ID `0x000`.
- ESP32 repeats the zero-token inventory query with a changing sequence during the discovery window. A single query is insufficient: two gateways can choose the same slot, and classic CAN cannot arbitrate different payloads with the same identifier.
- ESP32 may assign nodes as F0 frames arrive, but readiness still requires matching F2. HELLO does not substitute for F2.
- Hardware remains a prerequisite: with power off, a correctly terminated CAN bus normally measures about 60 ohms between CANH and CANL. Three enabled 120-ohm terminators measure about 40 ohms and can make every node disappear when an extra board is connected. Only the two physical ends of the bus should be terminated.

## SECTION 2.2: CAN ID Mapping After Assignment

After F2 confirmation, the assigned node_id determines all subsequent CAN IDs:

| Purpose | I2C Gateway | UART Gateway |
|---------|------------|--------------|
| ESP32 command TX | `0x200 + node` | `0x200 + node` (START) |
| Gateway status RX | `0x100 + node` | `0x100 + node` |
| UART data fragment | N/A | `0x300 + node` (DATA) |
| UART RX uplink | N/A | `0x400 + node` |
| Transport ACK | `0x500 + node` | `0x500 + node` |
| Health HELLO | `0x700 + node` | `0x700 + node` |

## SECTION 2.3: I2C Transport Details

**Register write (CMD_WRITE_REG = 0x03):** frame layout is `[0x03, addr, reg, len, data0..data3]`, so `len` is 1..4. ESP32 sends on `0x200+node`. CH32 returns a write-status frame on `0x100+node`, then ACK `[cmd,result,node_id,device_type,...]` on `0x500+node`. The transaction succeeds only when both responses match and both report success.

**Register read — two cases based on length:**

- Legacy mode is used only when `request_id==0` and `len<=3`; CH32 returns `STATUS_READ` (0x04) on `0x100+node`.
- Preferred mode uses a non-zero `request_id` for every read length; CH32 returns:
  - `STATUS_READ_CHUNK` (0x05) on `0x100+node`, each carrying 4 bytes
  - `STATUS_READ_DONE` (0x06) on `0x100+node` signals completion
- ESP32 must collect all chunks until STATUS_READ_DONE. Do not declare read complete after the first chunk.

**Bulk write (CMD_WRITE_MULTI = 0x08):** this is not a one-frame operation. Each CAN frame is `[0x08, addr, flags, chunk_len, payload0..payload3]`; `chunk_len` is 1..4, `START=0x01`, `END=0x02`, and the current CH32 buffer limit is 136 bytes. ESP32 sends the next fragment only after receiving matching `STATUS_WRITE_MULTI=0x09` and ACK for the current fragment. On retry, restart the entire transfer with START. Peripheral-specific code is responsible for resetting its own register/page/column cursor before replay so the retry is idempotent.

## SECTION 2.4: UART START/DATA/ACK Fragmentation Protocol

Current UART CH32 firmware uses reliable fragmentation, not single-frame passthrough:

1. ESP32 sends START on `0x200 + node`: contains transfer_id, total_length, CRC16, protocol_version, options.
2. CH32 returns START_ACK. ESP32 must NOT proceed without it.
3. ESP32 sends DATA fragments on `0x300 + node`: first 3 bytes are transfer_id (1 byte) + 16-bit sequence_number (2 bytes, big-endian). Remaining 5 bytes are UART payload.
4. CH32 reassembles in order, verifies total length and CRC16.
5. CH32 transmits the complete frame to UART, then returns COMPLETE_ACK on `0x500 + node`.

ESP32 must never skip START_ACK, and must never stuff all 8 bytes as UART payload.

**UART RX uplink:** CH32-UART is a bidirectional bridge. RX data from the downstream UART device is forwarded to ESP32 on `0x400 + node`. ESP32 registers a CAN RX callback for this CAN ID.

## SECTION 3: Stable Identity vs. Runtime Routing

- A CH32's stable identity is `device_type + token`. The token is derived from the CH32 chip UID (16 bits, little-endian in discovery frame `data[5..6]`). It does not change across power cycles.
- node_id is only a runtime CAN short address, dynamically assigned by ESP32. It may change across rediscovery.
- ESP32 maintains two table types: CH32 node table (keyed by token) and peripheral device table (keyed by kind+link+addr+node-reference).
- The device table references CH32 nodes by stable node reference, not by raw node_id. When node_id changes, only the node record is updated; device table pointers remain valid.

## SECTION 4: Power-Up Discovery Flow

- Phase 1: ESP32 scans local buses -- I2C: scan device addresses, match against knowledge base identification signatures -> add to device table. UART: initialize and mark as route-ready, do NOT directly add to device table. SPI: toggle each CS, read WHO_AM_I -> match signatures -> add to device table.
- Phase 2: ESP32 listens on CAN ID `0x000` for unassigned CH32 F0 broadcasts (data[0]=0xF0). Dispatches by device_type (0x01=I2C / 0x03=SPI / 0x04=UART). Deduplicates by token.
- Phase 3: For each new token, ESP32 sends F1 assignment frame using the current unified layout: node_id=data[1], token=data[4..5], device_type=data[6], protocol_version=data[7]. Wait for F2 confirmation.
- Phase 4: After I2C gateway assignment, ESP32 scans downstream I2C bus via CAN commands -> returns address list -> match identification signatures -> add to device table. SPI gateway: similar, but scans WHO_AM_I values per CS pin rather than address lines.
- Phase 5: UART gateway after assignment is registered as a candidate route. Do NOT assume what device is on the other end. Continuously listen on uplink; only after receiving valid protocol frames, confirm device type and add to device table.

## SECTION 5: I2C vs. SPI vs. UART Identification

- I2C: Has device address. The address itself is a strong identification clue, supplemented by WHO_AM_I secondary confirmation.
- SPI: No device address. Distinguished by CS pin. Each CS corresponds to one device. Identification flow: pull CS low -> read WHO_AM_I register -> match expected value in card.md -> confirm device type. CH32-SPI gateway must declare "how many CS lines, which GPIOs".
- UART: No address, no CS. Must rely on protocol frame content or explicit config. Two-layer management:
  - UART route: can send/receive bytes. NOT in device table.
  - UART device: has protocol evidence (e.g. VC02 command strings received) or explicit config. IN device table.
  - Before a UART device is identified, ESP32 may broadcast to all UART candidate routes. Once a specific device type is confirmed on a route, that route is bound and excluded from other device types' broadcasts.
- Modules without identification capability: use port-mapped identification. Mark "requires manual config" in card.md.

## SECTION 6: Device Table Maintenance

- Device table entry fields: module abbreviation, instance number, connection type (LOCAL_I2C / CH32_I2C / LOCAL_SPI / CH32_SPI / LOCAL_UART / CH32_UART), comm params, reference to stable CH32 node, online status, last-active timestamp, health stats (ok_count / error_count).
- Each device maintains its own state: present -> ready -> valid. Failure affects only itself; never blocks the main loop.
- Dedup key: local devices use kind + link + addr (SPI addr = CS number). Bridged devices use kind + link + addr + stable CH32 node reference. Do NOT use kind+addr alone, or devices with the same address/CS behind different CH32s will be incorrectly merged.
- Offline detection: consecutive comm failures reaching threshold mark device offline. Offline devices are no longer polled periodically.
- Device table does NOT proactively delete entries -- even offline devices keep their records. Upper layer decides based on error_count and offline flag.

## SECTION 7: Incremental Merge for Rediscovery

- New discovery results go into a temporary container (fresh_nodes). Never mix with the live node table during collection.
- Match CH32 gateways by `device_type + token`: identity exists -> update node in place, do not change its array position; identity new -> append. Within a type-specific table token alone is sufficient, but shared discovery code must keep device_type in the key.
- Corresponding code patterns: `merge_stable_i2c_node()` for I2C nodes; `uart_find_node_by_token()` for UART nodes.
- Rediscovery failure or timeout -> retain old node table and old device table. System continues running.
- Never do a full-clear-and-rebuild: one CAN timeout must not cause all devices to vanish instantly.
- UART rediscovery extra constraint: do NOT send global F3 RELEASE (would affect working I2C and SPI nodes via shared CAN ID 0x000). Only listen for new F0s and incrementally assign new tokens.

### Adaptive runtime rediscovery cadence

- Power-up performs discovery immediately; do not wait for the periodic timer.
- While one or more required module roles are unbound/offline, run incremental
  rediscovery no more frequently than once every 10 seconds.
- When every required role is online, extend incremental rediscovery to once
  every 30 seconds.
- Determine completeness from the roles declared by the composition (for
  example range sensor, display and speech), not merely from the number of CH32
  nodes or I2C addresses found.
- Ten seconds is the minimum normal runtime rediscovery interval.  Do not use an
  unconditional 1-5 second full or incremental discovery loop.
- A failed module enters local probe/rebind recovery.  Healthy module handles,
  stable node records and the shared CAN core remain active.
- Rediscovery is background maintenance and must not suspend normal module
  operation.  Do not hold an application-wide transport mutex for the complete
  discovery, downstream scan and initialization sequence.

## SECTION 7.1: Downstream Presence Evidence and Address Snapshots

- `ch32_i2c_multi_node_t.i2c_addrs[]` is a discovery/scan snapshot.  It may be
  empty, stale or truncated by the node record capacity.
- `ch32_i2c_multi_probe()` reports current downstream presence and is the
  authoritative presence check during module initialization.
- A successful probe does not imply that the gateway API mutates
  `i2c_addrs[]`; read the real API signature and implementation.
- Every CH32-I2C module driver performs its own live probe during init after
  validating the stable node reference.  An example may probe to select a
  candidate, but that does not replace driver validation.
- A driver must not reject a device that answered the current live probe merely
  because its address is absent from an older snapshot.
- If the snapshot must be refreshed, use an explicit generic gateway scan/update
  API.  Concrete module drivers must not privately edit discovery-layer data.

## SECTION 8: Device Discovery & Knowledge Base Relationship

- The knowledge base card.md provides the "identity template" for a device -- what comm interface, how to identify (I2C address + WHO_AM_I expected value / SPI CS + WHO_AM_I / UART protocol command signature / manual config needed).
- The discovery flow uses identity templates to match against devices actually found on buses. Match -> register. No match -> mark unknown.
- If a module's card.md is not yet in the knowledge base, it cannot be identified even if physically present on a bus. Ingest first, wire later.

## SECTION 9: ESP32-Side Code Constraints

- ESP32 must not turn "wiring assumptions" into "device facts". Device facts come from scan results, protocol responses, stable config.
- The three gateway discovery channels share CAN ID `0x000` but must be parsed by device_type and respective field layouts. Never cross-process (e.g. don't hand a UART node to the I2C allocator).
- AI-generated code must not invent protocols -- it must follow the real CH32 firmware frame formats (F0 field layout, F1 assignment frame node_id position per gateway type, CAN ID mapping rules, per-gateway transport procedures).
- CAN communication failure must distinguish: F1 TX failure (CAN frame not sent) vs. F2 timeout (CH32 didn't respond) vs. I2C scan failure (gateway assigned but downstream bus dead) vs. device init failure (device present but config failed). Log each case distinctly.

## SECTION 10: Bridge Driver — How to Obtain ch32_node_id

Bridge drivers (`_bridge.c`) do NOT discover CH32 nodes. The device discovery layer handles all F0/F1/F2 logic. The driver only consumes the result:

- Prefer passing a pointer/reference to the stable CH32 node record in cfg, rather than copying a raw `ch32_node_id` into the driver. The discovery/device-management layer owns this record; card.md never supplies a runtime ID.
- The stable node record contains `device_type`, token, current `node_id`, and F2-confirmed `ready` state. Driver `_init()` validates the reference, `ready`, token, and the node_id range for that gateway type.
- During rediscovery, update the stable node record in place. Existing device handles then follow the new runtime node_id without being deleted and recreated. If an older API can only accept a raw node_id, the device manager must rebind/reinitialize that driver after an ID change.
- Bridge timeout is typically 1.5-2x direct timeout. Exact value in card.md's "Bridge Constraints" field.

## SECTION 11: Verified ESP32 I2C Dynamic-ID Reference

Repository root: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM`

- Shared CAN/TWAI core: `components_esp32wroom/ch32_can_gateway_core/`
- I2C discovery and transport implementation: `components_esp32wroom/ch32_i2c_multi_gateway_final/ch32_i2c_multi_gateway_final.c`
- Public stable-node/API definition: `components_esp32wroom/ch32_i2c_multi_gateway_final/include/ch32_i2c_multi_gateway_final.h`
- Temporary discovery -> stable in-place merge usage: `examples_ch32/ssd1315_ch32_test/main/main.c`

These paths are reference implementations for the dynamic-ID architecture only: zero-token F0 inventory query, F0 deduplication by `device_type+token`, free-ID allocation, F1 token echo, F2-only readiness, stable node references, collision handling, and incremental rediscovery. They are not an SSD1315/OLED protocol template. For another module, reuse the shared gateway APIs and stable-node ownership pattern; obtain that module's downstream commands, registers, payload limits, timeout and observable behavior from its own card/datasheet and the authoritative CH32 firmware.

## SECTION 12: Verified ESP32 UART Dynamic-ID Reference

Repository root: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM`

- Shared UART discovery/transport: `components_esp32wroom/ch32_uart_dynamic_gateway_final/`
- Runtime inventory and stable node-table usage: `examples_ch32/syn6288_ch32_test/main/main.c`
- Authoritative CH32 UART peer: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/main.c`

Use these paths only for the dynamic gateway architecture: UART node range allocation, token-derived collision slots, F0/F1/F2, F2-confirmed readiness, stable node references, periodic incremental inventory, and START_ACK -> DATA -> COMPLETE_ACK transport sequencing. The SYN6288E frame bytes, GBK text, one-second speech interval and audio behavior are module-specific and must not be copied into another UART module.

The OLED bridge reference in Section 11 and the SYN bridge reference in this section are complementary examples of I2C and UART gateway discovery. Neither is a generic template for the downstream peripheral's registers, commands, payloads, timeouts, initialization sequence, or expected phenomenon.

## SECTION 13: Shared CAN Core Ownership

`components_esp32wroom/ch32_can_gateway_core/` is transport-neutral infrastructure. It alone owns TWAI installation/start/recovery, the single receive task, observer/I2C/UART routing queues, CAN transmit and the cross-transport transaction mutex.

- `ch32_i2c_multi_gateway_final` depends on the core and implements only I2C discovery, commands, status and ACK parsing.
- `ch32_uart_dynamic_gateway_final` depends on the core and implements only UART discovery, START/DATA/ACK and RX uplink parsing.
- UART must not depend on the I2C protocol component.
- Module drivers must not depend on the core directly; they depend on their protocol layer and consume stable node references.
- A multi-frame I2C or UART operation must hold the core transaction lock for the complete transaction, not for individual CAN frames.

### Required receive routing isolation

I2C and UART protocol layers must have independent discovery/control queues.
They must never compete to consume a single observer queue. The shared core
routes frames as follows:

| Frame | Routing discriminator | Destination |
|------|------------------------|-------------|
| F0 on `0x000` | `data[1]` device_type | Matching I2C/UART discovery queue |
| F2 on `0x000` | `data[6]` device_type | Matching I2C/UART discovery queue |
| HELLO on `0x700+node` | `data[0]` device_type | Matching discovery/health queue |
| I2C STATUS on `0x100+node` | CAN ID range | I2C control queue |
| UART RX on `0x400+node` | CAN ID range | UART receive/control queue |
| ACK on `0x500+node` | Real per-protocol ACK layout | I2C or UART control queue |

Do not assume that `data[3]` in every `0x500+node` ACK is device_type. The
current I2C ACK contains device_type there, while the verified UART transport
ACK uses `data[3]` as its result field and identifies the operation through the
ACK phase (`0x30`..`0x34`) and source-ID fields. Routing must follow the actual
firmware format. Unifying these layouts later requires a coordinated protocol
version change on CH32 and ESP32.

Portable bridge copy sets:

- I2C module: `ch32_can_gateway_core/` + `ch32_i2c_multi_gateway_final/` + module bridge driver + example.
- UART module: `ch32_can_gateway_core/` + `ch32_uart_dynamic_gateway_final/` + module bridge driver + example.
