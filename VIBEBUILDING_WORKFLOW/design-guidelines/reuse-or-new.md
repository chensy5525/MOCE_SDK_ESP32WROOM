# Driver Independence and Reuse Decision Rules

## SECTION 1: Core Principle

- Platform layers are reused; concrete module drivers are independent.
- A single-module generation task creates a complete driver and example for the
  requested module and connection mode. Existing module drivers are references,
  not dependencies.
- A multi-module example may reuse a verified driver only when it works without
  source, public-API, cfg, protocol or behavior changes.
- If a driver cannot be reused unchanged, create a complete new driver and keep
  all new-module-specific behavior in that driver's directory.

## SECTION 2: Allowed Reuse

- Always reuse platform infrastructure: BSP, the shared CAN core and the I2C or
  UART gateway protocol layer.
- In a multi-module example, reuse an already verified driver unchanged and
  create additional instances through cfg when it already supports the exact
  chip, interface, protocol and required behavior.
- Pure hardware-independent utilities such as CRC, encoding, fonts or numeric
  conversion may be shared when their API and ownership are explicit.
- Direct and CH32-bridge packages remain independent because their cfg and
  transport dependencies differ, even when they operate the same chip.

## SECTION 3: Must-Create-New Scenarios

- Different chip model: even if functionally similar (e.g. both are I2C temperature sensors), register addresses, data formats, and conversion formulas differ. Must write a new driver. Do not stuff TMP117 register addresses into the BMP280 driver.
- Different comm interface: same function but different interface (e.g. a module has both I2C and SPI variants). Driver structures differ significantly; separate files needed. Internal data conversion logic may be extracted as shared functions.
- Same chip but firmware/revision differences causing register incompatibility: judge by scope -- minor register address offsets can be handled with macro conditional compilation; major differences warrant a new file.
- Same category "sensor" but different subtype: data-type sensor driver structure (read register -> convert to physical) and event-type sensor driver structure (monitor state change -> return event) are fundamentally different. Do not reuse.
- Any case that requires changing an existing driver's register map, protocol,
  initialization, cfg, state machine or module behavior requires a new driver.

## SECTION 4: Reference Without Dependency

- A new driver may follow a verified driver's directory layout, CMake structure,
  BSP/gateway API usage, stable-node handling, logging and error paths.
- Do not copy another module's register constants, initialization sequence,
  command bytes, device state machine or module-specific retry behavior.
- Do not create a new driver that wraps or depends on another concrete module
  driver merely to replace part of its behavior.

## SECTION 5: Decision Flow

- Single-module task -> generate a complete independent driver and example for
  the requested mode, using existing code only as a structural reference.
- Multi-module task -> locate each previously verified single-module driver.
- Driver works unchanged -> reuse it and configure instances in the example.
- Any driver change would be required -> create a new complete driver first,
  verify it as a single module, then integrate it.
- "Create first" means a separately authorized single-module generation or
  maintenance task.  The composition task reports the exact gap and stops that
  integration path; it must not silently expand into card/direct/bridge work.
- Resume composition only after the missing or corrected package is independently
  available and confirmed for the requested connection mode.

## SECTION 6: Forbidden Partial Reuse

- Do not make a new module driver depend on an unrelated concrete module driver.
- Do not split one module's proprietary behavior across another module's folder.
- Do not add an ever-growing chip-selection switch to an existing driver unless
  the user explicitly defines those parts as one compatible product family.
- Reuse never skips compilation and hardware validation in the new composition.
