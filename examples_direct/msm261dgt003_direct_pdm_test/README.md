# msm261dgt003_direct_pdm_test

Minimal direct PDM microphone example for the revised ESP32-WROOM board and
MSM261DGT003 module.

Wiring follows the current board/module schematics:

- module VCC -> 3.3 V
- module GND -> GND
- module SCK -> GPIO18 / `I2S_SCK`
- module SD -> GPIO2 / `I2S_SD`
- module L/R selection must match `MSM261DGT003_DIRECT_PDM_SELECT_LOW`

The example starts 44.1 kHz signed 16-bit mono capture and periodically logs
the peak and mean absolute value from a 512-sample block. A louder nearby sound should
increase both values; this is a functional signal check, not calibrated SPL.

Build from the repository root in an initialized ESP-IDF shell:

```powershell
.\tools\build.ps1 examples_direct/msm261dgt003_direct_pdm_test esp32 my_board_esp32wroom
```

Hardware validation requires explicit flash authorization. Before flashing,
confirm that GPIO18 is not being used as SPI SCK and verify GPIO2 cold-boot and
download-mode behavior with the module connected.
