# SDK Reference — Where to Find BSP APIs, Protocol Definitions, and CH32 Firmware

When generating a module driver, DO NOT guess protocol constants, CAN IDs, or BSP function signatures. Look them up in the SDK using the paths below.

---

## SECTION 1 — BSP Layer (Board Support Package)

All hardware operations MUST use the BSP abstraction. NEVER call esp-idf low-level APIs directly.

```
MY_ESP32WROOM/
├── bsp/
│   ├── bsp_board/include/board.h       ← Pin map for the board (BOARD_CAN_TX_GPIO, BOARD_I2C_SDA, etc.)
│   ├── bsp_i2c/include/bsp_i2c.h       ← I2C init, bsp_i2c_add_device_7bit(), bsp_i2c_write_reg(), bsp_i2c_read_reg()
│   ├── bsp_uart/include/bsp_uart.h     ← UART init, bsp_uart_send(), bsp_uart_recv()
│   ├── bsp_gpio/include/bsp_gpio.h     ← GPIO init, set_level, get_level
│   └── bsp_pwm/include/bsp_pwm.h       ← PWM init, set_duty
│
├── boards/my_board_esp32wroom/
│   └── board.h                         ← GPIO assignment (I2C pins, UART pins, CAN pins, etc.)
```

### When generating a direct-connect driver

- Read `bsp/bsp_i2c/include/bsp_i2c.h` to get the actual I2C API signatures.
- Read `boards/my_board_esp32wroom/board.h` for pin definitions.
- Do NOT include esp-idf `driver/i2c.h` or `driver/gpio.h` directly — use `bsp_i2c.h` and `bsp_gpio.h`.
- Keep ESP-IDF `driver/*` types out of public module headers where practical.
  Include `board.h` and the required `bsp_<transport>.h` in the direct
  implementation file, and read the current header before using any signature.

### When generating a bridge (CH32) driver

- ESP32-side bridge drivers do NOT call `bsp_i2c_write_reg()` directly and do not own TWAI reception/discovery. They consume a stable CH32 node reference from the device-management layer and call the shared CH32-I2C gateway APIs.
- The verified ESP32 I2C dynamic-ID reference paths are listed in Section 3. Use them for discovery/session ownership; do not copy their SSD1315-specific display behavior into another module.
- CH32 firmware reference exists (see Section 4).

---

## SECTION 2 — Bridge Protocol Constants (Per-Device CH32 Gateways)

All CH32 transports share the generic CAN core below. It is the only component allowed to install TWAI, call `twai_receive()`, own routing queues, recover bus-off, and serialize multi-frame transactions:

```
MY_ESP32WROOM/components_esp32wroom/ch32_can_gateway_core/
```

I2C modules use the I2C protocol/session layer below; do not create a second per-module implementation of F0/F1/F2:

```
MY_ESP32WROOM/components_esp32wroom/ch32_i2c_multi_gateway_final/
```

The module-specific bridge driver remains in `components_ch32/ch32_<module>_gateway/`, but it should contain only peripheral behavior and translate operations through the shared gateway API. Protocol constants come from the authoritative CH32 firmware; do not duplicate them in every module header unless they are truly module-specific.

| Concern | Owner / source |
|---------|----------------|
| Gateway device_type, F0/F1/F2, CAN ID mapping | Shared gateway + `device-discovery.md` |
| I2C command/status layout and payload limits | Final CH32 I2C firmware; implemented once in the shared gateway |
| Peripheral registers, initialization and retry cursor reset | Module bridge driver + its card/datasheet |
| Stable node lifetime and rediscovery merge | Device manager/example layer; never the module driver |

### Important: I2C vs UART transport

- **I2C**: short register/raw operations fit one command frame, but reads can return chunk frames and `WRITE_MULTI` fragments payload into 4-byte chunks with START/END flags. Every command/fragment requires matching status and ACK.
- **UART**: Uses START/DATA/ACK fragmentation protocol. Payloads larger than one CAN frame are split across multiple DATA frames.

Full protocol specification is in `design-guidelines/device-discovery.md`.

---

## SECTION 3 — CAN Communication and Verified Dynamic-ID Reference

The following code has been aligned with the final CH32 I2C and UART gateways and is the ESP32 reference for dynamic allocation and shared CAN ownership:

```
C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM/
  components_esp32wroom/ch32_can_gateway_core/ch32_can_gateway_core.c
  components_esp32wroom/ch32_can_gateway_core/include/ch32_can_gateway_core.h
  components_esp32wroom/ch32_i2c_multi_gateway_final/ch32_i2c_multi_gateway_final.c
  components_esp32wroom/ch32_i2c_multi_gateway_final/include/ch32_i2c_multi_gateway_final.h
  examples_ch32/ssd1315_ch32_test/main/main.c
  components_esp32wroom/ch32_uart_dynamic_gateway_final/ch32_uart_dynamic_gateway_final.c
  components_esp32wroom/ch32_uart_dynamic_gateway_final/include/ch32_uart_dynamic_gateway_final.h
  examples_ch32/syn6288_ch32_test/main/main.c
```

Reference scope is deliberately limited: use the core for single TWAI ownership, queue routing, bus recovery and cross-transport transaction locking; use the I2C/UART layers for zero-token inventory F0, `device_type+token` identity, free-ID allocation, F1/F2 validation, stable node records, collision backoff, and incremental rediscovery. The examples happen to drive an OLED and a SYN6288E, but their SSD1315 registers/framebuffer/rendering and SYN6288E speech frames/timing are not reference logic for another module.

Dependency rule: `ch32_uart_dynamic_gateway_final` must depend directly on `ch32_can_gateway_core`; it must never depend on `ch32_i2c_multi_gateway_final`. Likewise, neither protocol layer may install TWAI or create another CAN receive task.

Module-driver rule: `ch32_can_gateway_core` is a dependency of protocol layers,
not a direct module-driver API. A module driver must not bypass its I2C/UART
gateway layer to construct CAN frames.

### Verified dependency trees

```
SSD1315 direct:
  ssd1315_direct -> bsp_i2c

SYN6288E direct:
  syn6288e_direct -> bsp_uart + bsp_board

SSD1315 bridge:
  ch32_ssd1315_gateway
    -> ch32_i2c_multi_gateway_final
      -> ch32_can_gateway_core

SYN6288E bridge:
  ch32_syn6288_gateway
    -> ch32_uart_dynamic_gateway_final
      -> ch32_can_gateway_core
```

### Verified module path inventory

Repository root: `C:/Users/chenqing/Desktop/GITHUB_LOCAL/MY_ESP32WROOM`

| Module/mode | Driver | Example |
|-------------|--------|---------|
| SSD1315 direct | `components_direct/ssd1315_direct/` | `examples_direct/ssd1315_direct_test/` |
| SSD1315 CH32 bridge | `components_ch32/ch32_ssd1315_gateway/` | `examples_ch32/ssd1315_ch32_test/` |
| SYN6288E direct | `components_direct/syn6288e_direct/` | `examples_direct/syn6288e_direct_test/` |
| SYN6288E CH32 bridge | `components_ch32/ch32_syn6288_gateway/` | `examples_ch32/syn6288_ch32_test/` |

### Deterministic reference selection for new modules

The reference is selected by physical transport, not by subjective functional
similarity:

| New module transport | Mandatory verified reference |
|----------------------|------------------------------|
| ESP32 direct I2C | `ssd1315_direct` driver + `ssd1315_direct_test` example |
| ESP32 direct UART | `syn6288e_direct` driver + `syn6288e_direct_test` example |
| CH32 bridge I2C | CAN core + I2C gateway + SSD1315 bridge driver/example |
| CH32 bridge UART | CAN core + UART gateway + SYN6288E bridge driver/example |

Use the direct references for BSP boundary, component layout, error/logging
structure and example organization.  They contain no CH32 dynamic-ID logic.
Use the bridge references for shared CAN ownership, F0/F1/F2, stable-node
ownership, gateway API calls and example discovery flow.

Do not copy the reference module's downstream behavior. SSD1315 registers,
framebuffer/rendering and refresh policy are OLED-specific. SYN6288E frame
bytes, GBK encoding, speech interval and audio behavior are SYN-specific. The
new module's behavior comes only from its `card.md`, datasheet and authoritative
device/CH32 firmware.

When the shared gateway must be corrected or extended, verify it against:

1. **Protocol spec**: `design-guidelines/device-discovery.md` — F0/F1/F2 frame layout, CAN ID mapping, I2C/UART command formats
2. **CH32 firmware**: Section 4 below — to verify which command codes the CH32 actually handles
3. **BSP layer**: Section 1 — for CAN pin assignment (`BOARD_CAN_TX_GPIO`, `BOARD_CAN_RX_GPIO`)

### Standard CAN ID mapping (defined in device-discovery.md)

| Range | Purpose |
|-------|---------|
| 0x000 | Discovery (F0 broadcast from unregistered CH32, F1 assignment from ESP32) |
| 0x100 + node_id | Status frames |
| 0x200 + node_id | I2C command frames |
| 0x300 + node_id | UART DATA fragments (gateway-specific) |
| 0x400 + node_id | UART RX uplink (gateway-specific) |
| 0x500 + node_id | ACK frames (ALL device types) |
| 0x700 + node_id | HELLO/HEARTBEAT frames (ALL device types) |

### Sending a CAN frame (gateway/core implementation reference only)

The following pattern is for maintaining the shared CAN core or gateway
protocol components. A module driver must not implement it directly.

```
1. Build 8-byte data array per protocol spec
2. Create twai_message_t: .identifier = CAN_ID, .data_length_code = 8, .data = { ... }
3. Call twai_transmit(&msg, pdMS_TO_TICKS(timeout_ms))
4. The single shared RX task routes frames to queues; a module driver must not call `twai_receive()` itself.
5. For I2C, require the matching status frame at `0x100+node_id` and ACK at `0x500+node_id`; ACK alone is insufficient.
```

---

## SECTION 4 — CH32 Firmware (Authoritative Source for Command Codes)

CH32 firmware reference code is in the MOCE_SDK_CH32 repository. These are the authoritative source for what command codes each gateway type handles:

```
MOCE_SDK_CH32/examples_final/CH32_I2C_gateway_dynamic/   ← authoritative I2C gateway firmware
MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/  ← UART gateway firmware
```

`examples_final/CH32_UART_gateway_dynamic` is the verified UART architecture reference. Do not change it as a side effect of generating an unrelated module. Use it to keep token derivation, F0/F1/F2 byte layout, assignment validation, collision backoff, HELLO behavior, node state and CAN bitrate aligned. Any change to the shared discovery frame contract requires an explicit coordinated protocol version change across CH32 and ESP32.

### When generating an ESP32 bridge driver, read the CH32 firmware to extract:

- Which command codes the CH32 actually handles (current I2C: 0x01 SCAN, 0x02 PROBE, 0x03 WRITE_REG, 0x04 READ_REGS, 0x05 WRITE_RAW, 0x06 WRITE_READ, 0x07 SET_SPEED, 0x08 WRITE_MULTI)
- Expected data layout per command (which bytes are register address, which are payload)
- ACK format (what the CH32 echoes back on success/failure)
- Discovery protocol implementation (F0 broadcast, F2 confirmation)
- Transport extensions such as WRITE_MULTI fragmentation; do not assume WRITE_RAW is suitable for a bulk display transfer

### When writing ESP32-side gateway code

1. **First**, read the CH32 firmware for the relevant gateway type to extract the exact command codes and data layouts.
2. **Then**, reuse/extend the shared ESP32 gateway component when the transport needs a missing generic operation. Do not fork discovery or TWAI ownership into the module driver.
3. **Then**, implement only the module-specific bridge behavior in `components_ch32/ch32_<module>_gateway/`, accepting the stable node reference through cfg.

---

## SECTION 5 — Output Paths for Generated Code

When generating a new module driver, place the output at the following paths:

### Bridge (CH32) drivers

```
Driver:   MY_ESP32WROOM/components_ch32/ch32_<module>_gateway/
Example:  MY_ESP32WROOM/examples_ch32/<module>_ch32_test/
```

### Direct-connect drivers

```
Driver:   MY_ESP32WROOM/components_direct/<module>_direct/
Example:  MY_ESP32WROOM/examples_direct/<module>_direct_test/
```

Naming convention for driver folders:
- Bridge: `ch32_<module>_gateway` (lowercase, underscores)
- Direct: `<module>_direct` (lowercase, underscores)

Each driver folder must contain:
- `<name>.c` — implementation
- `CMakeLists.txt` — build definition
- `include/<name>.h` — public header

---

## SECTION 6 — Module Datasheets (Chip Specifications)

Hardware datasheets and reference images are in:

```
MY_ESP32WROOM/docs/
```

Files include PDFs and screenshots showing pinouts, I2C addresses, and register maps. Consult these when filling in `card.md` fields like WHO_AM_I register values, I2C addresses, and timing parameters.

---

## SECTION 7 — Module Context Files

Some modules have pre-existing context files with hardware details:

```
MY_ESP32WROOM/context/
```

Examples:
- `context/context_oled_ssd1315.md` — SSD1315 OLED specs
- `context/context_laser_distance_vl53l0x_v2.md` — VL53L0X specs
- `context/context_mpu6050.md` — MPU6050 specs
- `context/context_esp32_wroom_32e_n4_board.md` — ESP32 module specs
- `context/context_esp32_adapter_board.md` — adapter board specs

These files contain pre-verified hardware parameters (I2C addresses, pinouts, register values). When generating a `card.md` for a module that has a context file, extract data from the context file — do NOT invent values.

If the context file is outdated or conflicts with the physical hardware, trust the physical hardware and update the context file.

---

## SECTION 8 — Quick Lookup by Task

| Task | Look here |
|------|-----------|
| I2C/UART/GPIO function signatures | `MY_ESP32WROOM/bsp/bsp_xxx/include/bsp_xxx.h` |
| Pin definitions | `MY_ESP32WROOM/boards/my_board_esp32wroom/board.h` |
| CAN ID mapping, F0/F1/F2 protocol | Knowledge base: `design-guidelines/device-discovery.md` |
| Shared TWAI ownership, routing and transaction lock | `MY_ESP32WROOM/components_esp32wroom/ch32_can_gateway_core/` |
| I2C command/status codes and layouts | `MOCE_SDK_CH32/examples_final/CH32_I2C_gateway_dynamic/main.c` |
| ESP32 I2C dynamic-ID architecture | `MY_ESP32WROOM/components_esp32wroom/ch32_i2c_multi_gateway_final/` and `examples_ch32/ssd1315_ch32_test/main/main.c` (dynamic-ID pattern only) |
| UART command codes (START/DATA/ACK) | `MOCE_SDK_CH32/examples_final/CH32_UART_gateway_dynamic/main.c` |
| ESP32 UART dynamic-ID architecture | `MY_ESP32WROOM/components_esp32wroom/ch32_uart_dynamic_gateway_final/` and `examples_ch32/syn6288_ch32_test/main/main.c` (dynamic-ID pattern only) |
| Chip register maps, I2C address, WHO_AM_I | `MY_ESP32WROOM/context/context_xxx.md` or `MY_ESP32WROOM/docs/xxx.pdf` |
| Code generation constraints | Knowledge base: `design-guidelines/code-generation-rules.md` |
| Where to output generated code | `MY_ESP32WROOM/components_ch32/` or `components_direct/` (Section 5) |

### Authority depends on the fact being checked

| Fact | Authoritative source |
|------|----------------------|
| CH32 command codes and frame layout | Corresponding current CH32 firmware |
| ESP32 gateway public API | Current gateway protocol header and CMake file |
| ESP32 BSP signature | Current BSP header and implementation |
| Board pins and shared resources | Target board's `board.h` |
| Chip registers and timing | Chip/module datasheet |
| Verified runtime architecture | Formally built, flashed and tested example |
| Consolidated module summary | `card.md`, subordinate to sources above |

If documentation, templates and code disagree, report the conflicting field,
both sources, the authority decision and the documents that require updating.
Do not silently choose one or invent a compromise.

---

## SECTION 9 — Fast Reference Entry Points and Verified Gateway Capabilities

Use this section to avoid recursively reading whole component trees. Always
verify that the target repository still contains these files. Ignore `build/`,
generated `sdkconfig*`, binaries, logs and IDE metadata while reading reference
packages.

### Minimal direct-I2C read set

1. `components_direct/ssd1315_direct/include/ssd1315.h`
2. `components_direct/ssd1315_direct/CMakeLists.txt`
3. The BSP calls used in `components_direct/ssd1315_direct/ssd1315_direct.c`
4. `examples_direct/ssd1315_direct_test/CMakeLists.txt`
5. `examples_direct/ssd1315_direct_test/main/CMakeLists.txt`
6. `examples_direct/ssd1315_direct_test/main/main.c`
7. Current `bsp_i2c.h`, its implementation, board `board.h`, and target CMake

### Minimal direct-UART read set

Use the equivalent public header, CMake, implementation API-call sites and
example files under `syn6288e_direct` and `syn6288e_direct_test`, followed by
current `bsp_uart.h`, its implementation, board `board.h`, and target CMake.

### Minimal CH32-I2C read set

1. `ch32_can_gateway_core/include/ch32_can_gateway_core.h` and CMake
2. `ch32_i2c_multi_gateway_final/include/ch32_i2c_multi_gateway_final.h` and CMake
3. Only the gateway implementation functions for the operations the new module
   needs (search by public API name)
4. `ch32_ssd1315_gateway` public header/CMake and stable-node validation pattern
5. `examples_ch32/ssd1315_ch32_test/main/main.c` and both example CMake files
6. `CH32_I2C_gateway_dynamic/main.c` handlers for those same operations

### Minimal CH32-UART read set

Use the corresponding core header/CMake, UART gateway public header/CMake,
needed transport functions, SYN6288E stable-node/example organization, and the
matching START/DATA/ACK handlers in `CH32_UART_gateway_dynamic/main.c`.

### Verified current I2C gateway capability summary

| Public capability | Verified behavior | Driver implication |
|-------------------|-------------------|--------------------|
| `ch32_i2c_multi_probe()` | Probes an explicit downstream 7-bit address | Use for address presence before module identification |
| `ch32_i2c_multi_write_reg_to()` | Explicit address/register write; register payload is 1–4 bytes | Use for normal configuration-register writes |
| `ch32_i2c_multi_read_regs_from()` | Explicit address/register read, length 1–32 bytes; creates a non-zero request ID; collects 4-byte READ_CHUNK frames; succeeds only after matching READ_DONE and ACK with the full requested length | Suitable for coherent sensor bursts such as MPU6050's 14-byte sample; module drivers must not reproduce chunk assembly |
| `ch32_i2c_multi_write_multi_to()` | Up to the current CH32 136-byte buffer, fragmented into 1–4 byte chunks with START/END and per-fragment status/ACK | Use only when a module needs a generic bulk I2C transfer |
| `ch32_i2c_multi_set_speed_100k/400k()` | Reconfigures the downstream CH32 I2C bus to the named supported speed | Use only a speed allowed by `card.md` |
| `ch32_i2c_multi_discover_incremental()` | F0/F1/F2 dynamic discovery with F2 readiness and incremental results | Example/device layer owns it; module driver never calls it |

This table is a navigation accelerator, not a replacement for repository
truth. Re-read the public header and relevant implementation/CH32 handler when
an API signature changes, a required limit is different, a capability is not
listed, or any source conflicts.

### Build and validation handoff

Normal code generation stops after static checks and gives the human the build
command. Do not start a full build, flash or monitor by default. If the human
returns compiler/linker errors and asks for correction, fix them and verify the
result with an incremental `idf.py build`. See `code-generation-rules.md` Rule
16 for the controlling policy.
