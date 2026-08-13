# VIBEBUILDING Knowledge Base Index

## Directory Structure

```
├── INDEX.md                              This file -- entry point
│
├── _global/                              Global rules (always upload, AI must follow literally)
│   ├── board-profile.md                  ESP32-WROOM pin map, GPIOs, I2C/UART/SPI/CAN/PWM config, BSP API
│   ├── coding-standard.md                Code style, naming conventions, forbidden patterns
│   ├── error-codes.md                    Common error code definitions
│   ├── logging-system.md                 Log levels, module tag format, output conventions (incl. multi-module log patterns)
│   └── glossary.md                       Terminology
│
├── design-guidelines/                    Design guidelines (AI reads and follows as reference)
│   ├── board-classification.md           Board categories, roles, typical wiring
│   ├── sensor.md                         Sensor driver guidelines (incl. direct vs. bridge)
│   ├── actuator.md                       Actuator driver guidelines (incl. direct vs. bridge)
│   ├── driver.md                         Motor driver board guidelines (incl. direct vs. bridge)
│   ├── communication.md                  Communication module driver guidelines (incl. direct vs. bridge)
│   ├── device-discovery.md               Device discovery & session mgmt (F0/F1/F2 protocol, CAN ID map, I2C/UART transport)
│   ├── startup-optimization.md           Startup time optimization strategies
│   ├── reuse-or-new.md                   Reuse vs. new driver decision rules
│   ├── min-example-standard.md           Minimum example output standard
│   ├── repository-migration.md            Complete copy sets and post-copy verification
│   ├── module-codegen-prompts.md           Three-stage new-module workflow prompts
│   ├── composition-codegen-prompt.md       Existing-module composition workflow prompt
│   ├── code-generation-rules.md          AI code generation must-follow rules (multi-module constraints)
│   └── sdk-reference.md                 SDK file navigation — where to find protocol constants, BSP APIs, CAN patterns
│
├── _templates/                           Code templates (13 files: 4 .h + 8 .c + 1 card template)
│   ├── sensor.h                          Sensor API/configuration skeleton
│   ├── sensor_direct.c                   Sensor: direct connection to ESP32
│   ├── sensor_bridge.c                   Sensor: via CH32 CAN bridge
│   ├── actuator.h                        Actuator header (shared)
│   ├── actuator_direct.c                 Actuator: direct connection
│   ├── actuator_bridge.c                 Actuator: via CH32 CAN bridge
│   ├── driver.h                          Driver board header (shared)
│   ├── driver_direct.c                   Driver board: direct connection
│   ├── driver_bridge.c                   Driver board: via CH32 CAN bridge
│   ├── communication.h                   Communication module header (shared)
│   ├── communication_direct.c            Communication: direct connection
│   ├── communication_bridge.c            Communication: via CH32 CAN bridge (UART gateway)
│   └── module-card-template.md           Module card template (with bridge fields)
│
│   Direct and bridged versions are independent software packages, each with
│   its own driver, public header, CMake definition and minimum example.
│   Their operation semantics should be consistent where practical, but their
│   cfg types and transport-specific public types do not have to be identical.
│
├── modules/                              Per-module data (load on demand)
│   └── <module_abbr>/
│       └── card.md                       Module card
│
├── compositions/                         Multi-module compositions (empty for now)
│
└── _archive/                             Deprecated entries
```

## Module Registry

| Abbreviation | Type | Interface | Connection | Status |
|-------------|------|-----------|------------|--------|
| MPU6050 | sensor-data | I2C | direct / bridged | card confirmed; implementation guide added |
| OLED (SSD1315) | hmi-output | I2C | direct / bridged | direct and bridge hardware phenomena verified |
| SYN6288E | hmi-output | UART | direct / bridged | direct and bridge speech phenomena reported working |
| VL53L0X | sensor-data | I2C | direct / bridged | physical identity and direct ranging verified |

## Usage

- Every CODEX session: always upload all files under `_global/`
- Per task: upload relevant `modules/<abbr>/card.md` on demand
- Before generating code: AI must read the matching template, corresponding design-guideline, AND `code-generation-rules.md`
- Specify which version to generate: `direct`, `bridge`, or `both`
- New module workflow: generate card.md -> AI self-check -> human hardware test -> ingest -> update this file
- If module info is incomplete: AI fills known fields and marks unknowns as `[待确认]`. It may continue only when the unknown field cannot materially change the generated hardware behavior; otherwise it requests confirmation.
- Bridge version description: see design-guidelines/<type>.md "Direct vs. Bridged Versions" section
- Protocol details (F0/F1/F2 frame layout, CAN ID mapping, I2C/UART transport): see `design-guidelines/device-discovery.md`
- SDK file paths (where to find BSP APIs, gateway protocol headers, CAN reference code): see `design-guidelines/sdk-reference.md`
- AI code constraints (don't hardcode, incremental rediscovery, UART vs I2C identification): see `design-guidelines/code-generation-rules.md`
- Multi-module composition generation must also follow Rule 17 in
  `design-guidelines/code-generation-rules.md`, Section 7/7.1 in
  `design-guidelines/device-discovery.md`, and Section 7 in
  `design-guidelines/min-example-standard.md`.  These define default FreeRTOS
  organization, neutral equal-priority application tasks, adaptive
  missing-role/all-online rediscovery (10s/30s), live I2C probe authority,
  local recovery and efficient latest-state output.

## Authoritative Scope

AI treats only these locations as normal authoritative knowledge sources:

- `_global/`
- `_templates/`
- `design-guidelines/`
- `modules/`
- `compositions/`
- `validation/` (when present)
- `INDEX.md`


## Knowledge-Base Write Policy

- This repository is curated knowledge, not a default code-output workspace.
  AI must not create files or directories here merely because they may be useful.
- The only normal automatic addition is
  `modules/<module>/card.md` during explicitly requested Stage 1 of the new
  single-module workflow. Creating that card also creates its module directory
  when necessary.
- Stage 2 direct code, Stage 3 CH32 bridge code, multi-module compositions,
  English source code, build files, generated SDK output, logs, extracted PDFs,
  temporary analysis and validation artifacts go only to the user-specified
  external code/output repository. They are not written under this knowledge
  base unless the current user request explicitly names a knowledge-base target.
- Existing knowledge files may be edited only when the user explicitly asks to
  update/correct the knowledge base or to ingest confirmed validation facts.
- Adding a new template, guideline, prompt, composition document, validation
  record, manifest, summary, checklist, archive entry or helper file requires
  explicit user authorization. Do not infer permission from a code-generation,
  debugging, research or hardware-validation request.
- If useful knowledge is discovered outside an authorized knowledge-base update,
  report the proposed change in chat and wait for authorization. Do not save it
  proactively.
- Never create temporary, staging, revision, build or SDK-output directories
  inside this repository. Use the system temporary directory or the explicitly
  named external repository and remove temporary artifacts when their task ends.

## Code Generation and Reuse Policy

- A new single-module task creates a complete independent driver package and a
  complete independent example package for the requested connection mode.
- Existing module drivers are references for structure, platform API use,
  logging and error handling; a new module driver must not depend on another
  concrete module driver.
- A multi-module example reuses a verified module driver only when that driver
  works unchanged. If protocol, registers, initialization, cfg or behavior must
  change, create a complete new driver instead of partially reusing the old one.
- Platform layers are intentionally shared: BSP components,
  `ch32_can_gateway_core`, `ch32_i2c_multi_gateway_final` and
  `ch32_uart_dynamic_gateway_final`.

## Generation Workflows

New-module ingestion and existing-module composition are separate workflows.
Do not enter one workflow from the other without a new explicit user task.

### New single-module workflow

This workflow has exactly three stages: `card.md + implementation guide` ->
direct package -> CH32 bridge package. Reusable prompts are in
`design-guidelines/module-codegen-prompts.md`.

1. Generate and confirm `card.md` from the module materials. Stage 1 also
   researches and records the module's implementation logic, source evidence,
   direct/bridge characteristics, complexity and validation risks. The same
   implementation guide must be presented directly in the final chat response.
   Stage 1 does not generate code.
2. Generate the direct package. Direct I2C uses the verified SSD1315 direct
   package as its mandatory structural/API reference; direct UART uses the
   verified SYN6288E direct package.
3. Generate the CH32 bridge package. CH32-I2C uses the verified OLED bridge
   chain as its mandatory dynamic-ID/gateway reference; CH32-UART uses the
   verified SYN6288E bridge chain.

Each stage is a separate user-authorized task. Generating a card does not
authorize direct code, and generating direct code does not authorize bridge
code.

### Existing-module composition workflow

This workflow directly combines already confirmed cards and already verified
drivers. Its reusable prompt is in
`design-guidelines/composition-codegen-prompt.md`.

- Do not generate or revise module cards as part of composition generation.
- Do not silently start a missing single-module direct/bridge stage.
- Reuse each verified driver unchanged. If a driver or generic gateway ability
  is absent or requires a protocol/public-API change, report the exact gap and
  stop that integration path. The user starts a separate single-module
  maintenance/generation task before composition resumes.
- Generate only the requested connection-mode composition and its project
  organization; do not mix direct and bridge modes unless explicitly requested.

Every stage still reads the target repository's current API/CMake definitions.
Reference code supplies architecture and API usage only; module behavior comes
from the current module card and authoritative technical material.

By default, each code-generation stage stops after writing the complete source
package and performing static boundary/structure checks. It reports the exact
manual build command and marks compilation and hardware validation as pending.
The human runs the first build; if an error is returned, AI fixes the error and
then performs an incremental compile to verify the correction. An explicit
request in the current task may override this default and request build, flash
or serial monitoring immediately. The controlling policy is
`design-guidelines/code-generation-rules.md` Rule 16.
