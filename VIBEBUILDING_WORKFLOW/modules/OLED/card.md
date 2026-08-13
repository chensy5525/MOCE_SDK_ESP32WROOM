# SSD1315 0.96-inch OLED Display Module Card

## Hardware Model
SSD1315-based IIC-0.96OLED module (Wisevision X096-2864KSWPG01-H30), 128 x 64 monochrome display

## Module Type
hmi-output

## Communication Interface
I2C, direct / bridged

## Comm Parameters

### I2C
- Device address: 0x3C (7-bit, fixed on this board; schematic label 0x78 is the 8-bit write address)
- Max speed: 400 kHz

## Pin List
- U3 pin 1: GND
- PIN_SSD1315_SCL = GPIO_22 (U3 pin 2, I2C_SCL, on-board 4.7 kΩ pull-up to 3.3 V)
- PIN_SSD1315_SDA = GPIO_21 (U3 pin 3, I2C_SDA, on-board 4.7 kΩ pull-up to 3.3 V)
- U3 pin 4: 3.3 V power input
- PIN_SSD1315_RST: not exposed on U3; RES# is handled by the on-board reset network

## Hardware Notes
- Power-up stabilization time: 100 ms recommended
- Enable pin: none exposed
- Range / thresholds: 128 x 64 pixels, 1/64 duty, monochrome white
- Special timing requirements: use 400 kHz I2C for practical full-screen refresh; after power-up send display off (0xAE), initialize the controller, clear all 1024 bytes of GDDRAM, then send display on (0xAF)
- Independent power supply: no; power the module from 3.3 V and share ground with the ESP32 or CH32 bridge
- Connector: WAFER-GH1.25-4PWB, 1.25 mm pitch, 4 pins
- Mechanical: PCB outline approximately 24.473 x 22.000 mm with four 2.2 mm non-plated mounting holes; supplied panel drawing is 24.7 x 16.6 x 1.3 mm; assembled height and weight are [待确认]
- Known issues: without the required initialization and GDDRAM clear sequence, the display may appear fully lit; refresh is slow below 400 kHz; some engineering files use the legacy SSD1306 label, but the confirmed production controller is SSD1315

## Identification Method
- Port-mapped identification: requires manual config; an I2C ACK at 0x3C confirms a responding device but does not uniquely identify an SSD1315

## Bridge Constraints
- Bridge timeout: 2000 ms (twice the board-profile direct I2C timeout of 1000 ms)
- Required constraints: the stable CH32 node reference is supplied by the discovery layer; the module driver does not discover or assign node IDs. Register/command writes use `CMD_WRITE_REG` (0x03). Framebuffer data uses `CMD_WRITE_MULTI` (0x08), with START=0x01, END=0x02 and at most 4 payload bytes per CAN frame. ESP32 and every CH32 node on the CAN bus use 500 kbit/s. CH32 downstream I2C is explicitly switched to 400 kHz. A failed bulk-write retry restarts from an explicitly programmed OLED page/column cursor.
- Recommended implementation: cache display pages, transmit only changed page ranges, throttle refreshes to at least 100 ms apart, and coalesce pending requests so only the latest framebuffer state is retained.

## Dependencies
- Direct software dependency: `bsp_i2c`
- Bridge protocol dependency: `ch32_i2c_multi_gateway_final`
- Shared CAN core: `ch32_can_gateway_core` (transitive dependency; the SSD1315 driver must not call it directly)
- Required CH32 firmware: `MOCE_SDK_CH32/examples_final/CH32_I2C_gateway_dynamic/`
- Typically used with: ESP32

## Software Package Paths
- Direct driver: `components_direct/ssd1315_direct/`
- Direct example: `examples_direct/ssd1315_direct_test/`
- Bridge driver: `components_ch32/ch32_ssd1315_gateway/`
- Bridge example: `examples_ch32/ssd1315_ch32_test/`

## Identification Confidence
- WHO_AM_I available: no
- Address-only identification: yes; an ACK at 0x3C is not unique proof of SSD1315
- Protocol confirmation: no standard identity command
- Manual configuration required: yes

## Authoritative Sources
- Datasheet/schematic: source package supplied with the OLED module
- CH32 firmware: `MOCE_SDK_CH32/examples_final/CH32_I2C_gateway_dynamic/main.c`
- Verified ESP32 direct example: `MY_ESP32WROOM/examples_direct/ssd1315_direct_test/`
- Verified ESP32 bridge example: `MY_ESP32WROOM/examples_ch32/ssd1315_ch32_test/`
- Hardware validation: direct and CH32-bridge display phenomena verified; exact date [待确认]

## Code Implementation Guide

### Operating Model
- Module behavior: command-driven framebuffer display. SSD1315 GDDRAM stores
  eight 128-byte pages; visible output changes only after command/data writes.
- Mental model: application rendering updates a local framebuffer, marks changed
  pages/regions dirty, and flushes only the newest changed content to the OLED.
- Initialization prerequisite: stable 3V3 and the 100 ms power-up wait; reset is
  handled by the module hardware because no reset pin is exposed.

### Initialization Sequence
1. Initialize I2C at 400 kHz and live-probe 7-bit address `0x3C`.
2. Wait for power stabilization, send display-off and the confirmed SSD1315
   controller configuration sequence.
3. Initialize addressing/page state, clear all 1024 GDDRAM bytes once, then send
   display-on.
4. Initialize the local framebuffer and dirty tracking before accepting draws.
- Order constraints: display remains off during controller setup and initial
  clear; normal operation must not repeat full clear for every text update.

### Runtime Data or Command Path
```text
application text/graphics -> local framebuffer update -> dirty comparison ->
page/region address commands -> bulk data transfer -> clear transmitted dirty bits
```
- Blocking points: page command plus bulk I2C/CAN transfer; every transaction is
  bounded and a failed page remains dirty for a later retry.
- Completion evidence: transport acknowledgement proves bytes reached the
  downstream interface, not that pixels are visually correct.

### Data Conversion and Validity
- Raw format: one bit per monochrome pixel, eight vertical pixels per page byte,
  128 bytes per page and eight pages.
- Conversion: glyph/graphic routines convert application content into page bytes;
  there is no sensor unit conversion.
- Validity checks: coordinates, page bounds, buffer length and transfer result.
- Invalid/out-of-range behavior: reject the draw/transfer with an error; do not
  write outside the fixed framebuffer.

### Timing and Scheduling Characteristics
- Power-up/config delay: 100 ms recommended before controller initialization.
- Measurement/update timing: not applicable; visible refresh is output-driven.
- Recommended effective rate: use the highest stable effective rate allowed by
  changed bytes and transport budget. The existing 100 ms bridge minimum is a
  coalescing bound, not a requirement to make the display visibly slow.
- Periodic schedule or minimum action gap: display refresh is a coalesced latest-
  state output; obsolete intermediate frames are discarded.
- Work that can proceed while waiting: rendering and other module tasks remain
  independent; shared transport locks cover only actual transfers.

### Lifecycle and Recovery
```text
uninitialized -> probing -> configuring -> online -> dirty/refreshing -> online
```
- Transient failure: retain dirty pages and retry with a bounded policy from an
  explicitly restored page/column cursor.
- Invalid draw: affects only that request.
- Consecutive communication failure: mark this display offline and locally
  re-probe/reinitialize it without restarting other modules.
- Identity limitation: ACK at `0x3C` confirms presence but not exact SSD1315
  identity; module selection remains explicit configuration.

### Direct Implementation Notes
- BSP/API boundary: `bsp_i2c` at 400 kHz through a per-device handle.
- Instance/buffer model: static instance/fixed pool with a 1024-byte framebuffer,
  dirty mask/regions and no periodic allocation.
- Polling/callback/task consideration: a bounded display consumer can coalesce
  requests; task separation is based on transfer blocking, not importance.
- Public API should expose clear/draw/refresh operations and transport-neutral
  coordinates/content, not ESP-IDF I2C types.

### CH32 Bridge Implementation Notes
- Required generic gateway: `ch32_i2c_multi_gateway_final`.
- Stable-node and live-presence validation: validate the F2-confirmed stable node
  and live-probe `0x3C`; an old `i2c_addrs[]` snapshot is not authoritative.
- Transfer expansion: command bytes use WRITE_REG and page data uses WRITE_MULTI
  with four-byte CAN payload chunks, so unchanged-page suppression is essential.
- Timeout/idempotency: after an ambiguous/failed page transfer, restore page and
  column commands before retransmission.
- Existing generic capability assessment: sufficient and previously verified.
- Discovery/business boundary: discovery owns the stable node; the OLED driver
  owns controller commands, framebuffer and dirty tracking only.

### Module-Specific Difficulties and Common Mistakes
- Distinguishing characteristic: a full screen is 1024 bytes and expands into
  many CAN fragments in bridge mode.
- Likely mistake: clearing or sending the complete framebuffer for every value;
  use fixed-width fields plus dirty page/region updates and depth-one latest state.
- Likely mistake: treating datasheet `0x78` as a 7-bit address; use `0x3C`.
- Likely mistake: accepting an old discovery snapshot as presence authority;
  live-probe during driver init.
- Hardware validation focus: orientation/remap, full clear, stale-character
  removal, changed-page refresh rate and recovery after disconnect.

### Implementation Complexity
- Direct implementation: medium.
- CH32 bridge implementation: high.
- Main complexity sources: framebuffer rendering, dirty tracking, controller
  addressing and CAN-fragment-expanded bulk transfers.
- Reusable existing capabilities: verified SSD1315 direct/bridge packages,
  `bsp_i2c`, WRITE_REG and WRITE_MULTI gateway operations.
- Possible generic capability gaps: none currently confirmed.

### Guide Evidence Classification
- Confirmed from user material: SSD1315 module, 128x64 geometry, 3V3 wiring,
  fixed `0x3C`, reset network and initialization/display constraints.
- Supplemented from official/current sources: SSD1315 command behavior and
  current CH32 WRITE_MULTI capability recorded by the existing card sources.
- Inference: latest-frame consumer and local-recovery structure follow the
  knowledge-base multi-module rules and transport expansion.
- Pending hardware verification: exact validation date and refresh-rate limit
  under each future multi-module traffic mix.

## Module-Specific Error Codes
- none; use the common error codes defined in `_global/error-codes.md`

## Changelog
- 2026-08-11: Initial module card created from the supplied schematic, BOM, Gerber, STEP model, panel datasheet, and user-provided operating notes
- 2026-08-11: Corrected the confirmed controller model and module abbreviation from SSD1306/OLED to SSD1315
- 2026-08-11: Applied board-profile I2C pins (SDA GPIO21, SCL GPIO22), set the CH32 bridge timeout to 2000 ms, and documented the bridge transport and refresh constraints
- 2026-08-11: Verified the bridge protocol against `CH32_I2C_gateway_dynamic/main.c`: WRITE_MULTI=0x08, four-byte chunks, START/END flags, and ACK layout
- 2026-08-12: Corrected the shared CAN bitrate to 500 kbit/s, recorded the real driver/example paths and separated required constraints from recommended refresh optimizations
- 2026-08-12: Added the reusable Code Implementation Guide with efficient highest-stable-rate refresh, dirty-region and live-probe requirements
