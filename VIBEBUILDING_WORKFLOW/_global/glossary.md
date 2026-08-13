# Glossary

## SECTION 1: Hardware Terms

- **Main Controller (主控)**: ESP32-WROOM. Runs main logic, manages all sub-modules.
- **CH32 / Bridge Chip (桥接芯片)**: CH32V203. Acts as CAN-to-I2C/UART/SPI forwarding board. No business logic.
- **Direct Connection (直连)**: Module wired directly to ESP32 bus (I2C/UART/SPI), no CH32 involved.
- **Bridged (桥接)**: Module attached downstream of a CH32. ESP32 accesses it indirectly via CAN through the CH32.
- **Downstream Module (下游模块)**: Module on the CH32 side.
- **Forwarding Board / Auxiliary Board (转发板/辅助板)**: Signal conversion or routing only, no business logic (e.g. level shifter, I2C MUX).

## SECTION 2: Software Terms

- **Module Abbreviation (模块缩写)**: 4-8 uppercase char unique tag. Used for function prefix, log tag, error code prefix, file naming.
- **Module Card (card.md)**: The per-module descriptor file in the knowledge base.
- **Minimal Example (最小例程)**: Simplest code verifying basic module function. Output is unified to serial print or OLED display.
- **Ingest (入库)**: After a module driver passes all checks, write its card.md into modules/ and update INDEX.md.

## SECTION 3: System Terms

- **Device Discovery (设备发现)**: On power-up, ESP32 scans local buses + dispatches CH32s via CAN to scan downstream buses, identifying all online devices.
- **Device Table (设备表)**: ESP32-maintained unified device view. Direct and bridged devices are equal; connection method is transparent.
- **Identification Signature (识别特征)**: The auto-identification basis declared in card.md (WHO_AM_I register, AT command response, or manual config).
- **Instance Number (实例号)**: Runtime-assigned number for multiple physical instances of the same module model. Increments from 0. Re-assigned on each power cycle.

## SECTION 4: Project Conventions

- **Single-Module Example (单模块例程)**: Involves one module + main controller only.
- **Composition Example (组合例程)**: Multiple modules working together.
- **Direct Version (直连版本)**: Example code variant where module connects directly to ESP32.
- **Bridge Version (桥接版本)**: Example code variant where module connects via CH32.
