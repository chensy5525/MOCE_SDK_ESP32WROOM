# oled_ssd1315

Direct ESP32-WROOM I2C driver for a 128x64 SSD1315 OLED at address `0x3C`.

This component is a standalone hardware diagnostic driver. It does not replace
the normal ESP32-WROOM -> CH32 gateway path.

## Public API

- `oled_ssd1315_default_config()`: fills the `0x3C`, 400 kHz defaults.
- `oled_ssd1315_init()`: initializes I2C, probes the address, configures SSD1315,
  clears GDDRAM, and enables the panel.
- `oled_ssd1315_clear()`: clears the RAM framebuffer.
- `oled_ssd1315_draw_ascii()`: draws 5x7 ASCII.
- `oled_ssd1315_draw_utf8()`: draws ASCII plus the built-in 16x16 Chinese glyphs.
- `oled_ssd1315_draw_uint()`: formats and draws an unsigned integer.
- `oled_ssd1315_refresh()`: transfers the complete framebuffer to the OLED.
- `oled_ssd1315_set_display()`: sends display on/off.
- `oled_ssd1315_get_status()`: exposes state and counters.

## Chinese Text Scope

The driver intentionally does not embed a complete Chinese font. It currently
contains only `你`, `好`, `显`, `示`, `正`, and `常`, requiring about 200 bytes
of glyph data. Add future glyphs to the table only when an application needs
them. Unsupported UTF-8 glyphs return `UNSUPPORTED_GLYPH` instead of displaying
uncontrolled data.

## Failure Behavior

- `ADDR_NOT_FOUND`: no I2C ACK at `0x3C`.
- `INIT_FAIL`: address responded, but configuration or initial clear failed.
- `COMM_FAIL`: a runtime I2C transfer failed.
- `OUT_OF_BOUNDS`: requested text does not fit the 128x64 framebuffer.
- `UNSUPPORTED_GLYPH`: the requested character is not in the small font table.

## Validation

- Host font lookup tests: passed.
- ESP-IDF compile: see the example validation result.
- Hardware display test: pending user verification.
