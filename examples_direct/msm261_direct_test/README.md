# msm261_direct_test

MSM261DGT003 PDM microphone direct minimum example for ESP32-WROOM.

## Observable behavior

- Captures 16 kHz, signed 16-bit, mono PCM through I2S0 PDM RX.
- Uses 128x PDM-to-PCM down-sampling, producing a 2.048 MHz PDM clock.
- Continuously prints 100 ms windows of peak, DC-removed RMS and dBFS on UART0.
- It does not recognize commands and does not report calibrated dB SPL.

Example output:

```text
[INF][MSM261] init OK, rate=16000Hz bits=16 window=100ms clk_gpio=18 data_gpio=2 slot=LOW
[INF][MSM261_EXAMPLE] valid=1 peak=8421 rms=1260.4 dbfs=-28.3 dc=12 clipped=0 ok=1 err=0
```

## Hardware

- Module `3.3V` -> board `3.3V`
- Module `GND` -> board `GND`
- Module `SCK` -> board `I2S_SCK` (`BOARD_PDM_CLK_GPIO`, GPIO18)
- Module `SD` -> board `I2S_SD` (`BOARD_PDM_DATA_GPIO`, GPIO2)
- Default slot is automatic: initialization probes LOW and HIGH and keeps the
  first slot that produces non-constant PCM data. A fixed slot can still be
  selected explicitly when required.

## Build

From the repository root:

```powershell
.\tools\build.ps1 examples_direct/msm261_direct_test esp32 my_board_esp32wroom
```

Compilation, flashing and hardware behavior were verified on the target board.
