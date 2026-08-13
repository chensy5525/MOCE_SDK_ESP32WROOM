# Error Code Definitions

## SECTION 1: Return Value Convention

- Return `0` for success
- Return negative value for error (the value is the error code)
- Callers do not need to decode which module produced an error -- the module abbreviation is already in the function name and log output

## SECTION 2: Common Error Codes (shared by all modules, defined in a common header)

- `ERR_TIMEOUT`        (-1)  Operation timed out
- `ERR_BUSY`           (-2)  Device busy
- `ERR_NOT_INIT`       (-3)  Module not initialized
- `ERR_INVALID_PARAM`  (-4)  Invalid parameter
- `ERR_NOT_SUPPORTED`  (-5)  Operation not supported
- `ERR_OVERFLOW`       (-6)  Buffer overflow
- `ERR_NO_DEVICE`      (-7)  Device not present / no response
- `ERR_HW_FAULT`       (-8)  Unrecoverable hardware fault

## SECTION 3: Module-Specific Error Codes

- Defined in each module's own header file
- Naming format: `ERR_<ABBR>_<MEANING>`
- Examples: `ERR_BMP280_OUT_OF_RANGE`, `ERR_RELAY_OVERCURRENT`
- Specific values and meanings documented in the module's card.md
- Use enum or #define; no mandatory encoding scheme. Callers identify errors by macro name.

## SECTION 4: Gateway-to-Module Error Mapping

| Gateway result/stage | Module-facing error |
|----------------------|---------------------|
| Transport timeout | `ERR_TIMEOUT` |
| Shared transaction lock busy | `ERR_BUSY` |
| Invalid cfg/node/argument | `ERR_INVALID_PARAM` |
| Downstream address does not respond | `ERR_NO_DEVICE` |
| CAN transmission or bridge protocol failure | `ERR_HW_FAULT` or a documented `ERR_<ABBR>_COMM` |
| F2 assignment not confirmed | Discovery-layer error; module driver is not initialized |

Mapping an internal result to a common error code must not erase diagnostics.
The log records the original layer, operation stage and gateway result.
