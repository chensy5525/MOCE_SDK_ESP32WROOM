# Coding Standard & Naming Conventions

## SECTION 1: Format & Syntax (hard rules, copy literally)

- Comment language: Chinese
- Macros: ALL_CAPS with underscores
- Constants: use `#define` or `const`. No magic numbers in code.
- Header guard: `#ifndef <FILENAME_UPPER>_H__` / `#define` / `#endif`

## SECTION 2: Naming (AI derives from rules)

- **Module abbreviation**: 4-8 uppercase chars, globally unique (e.g. `BMP280`, `RELAY`). Declared at ingest time. Used as: function prefix, log tag, error code macro prefix.
- **Function naming**: `<abbr>_<action>_<object>`, all lowercase with underscores. Example: `bmp280_read_temp`
- **File naming**: `<lowercase_abbr>.h` / `.c`. Example: `bmp280.h`
- **Variables**: snake_case. Global prefix `g_`, static prefix `s_`.
- **Pin macros**: `PIN_<ABBR>_<FUNCTION>`. Example: `PIN_BMP280_SDA`

## SECTION 3: Forbidden (hard rules, reject on violation)

- Default to static instances or a fixed-capacity instance pool.
- Dynamic allocation is allowed only when card.md explicitly permits it and
  defines the maximum instance count, allocation phase and release path.
- Never allocate memory in an ISR, periodic read/write loop or per-frame path.
  If permitted, allocation is limited to init/deinit and every failure path must
  release resources acquired by that init attempt.
- No `printf` inside ISR
- ISR must not exceed 10 lines
- No cross-file function declarations (every `.c` has exactly one corresponding `.h`)
