# Validation Context: Wi-Fi 06 alarm portal (2026-08-18)

## Current evidence

- ESP-IDF 6.0.2 target build: **passed** for classic ESP32 with DIO/40 MHz/4 MB.
- Application binary: `wifi_alarm_portal_test.bin`, size `0xC5390` bytes; the 1 MiB app
  partition reports `0x3AC70` bytes (23%) free.
- Bootloader binary: built successfully, size `0x65F0` bytes.
- Application SHA-256: `F3610479EE189F8990E74E48EA6642525585B347B05F081DC087B40D8B7C3CAC`.
- COM22 flash: **passed**. Esptool identified ESP32-D0WD-V3 revision 3.1 with a 40 MHz crystal;
  bootloader at `0x1000`, partition table at `0x8000`, and application at `0x10000` each passed
  written-data hash verification before hard reset.
- Boot/SoftAP bench state: **passed**. A bounded 15-second 115200 8N1 capture shows the expected
  project/version, controller handoff `07:00 enabled=0`, SoftAP mode, DHCP server at
  `192.168.4.1`, and `AlarmCar-06` ready without a reset loop or Wi-Fi/NVS initialization error.
- Client association/DHCP bench state: **passed**. Windows associated to BSSID
  `b4:bf:e9:2e:aa:7d`; the ESP log recorded client join and assigned `192.168.4.2`.
- HTTP page/API, invalid-request handling, and NVS persistence: **untested**. Automated client
  access was stopped at the user's request because switching the only WLAN adapter interrupted
  the user's network.
- Captive-portal DNS and automatic OS page redirection: **not implemented / untested**.
- Alarm scheduling, SD, audio decoding, I2S, amplifier, and speaker behavior: **untested**.

Raw evidence (ignored build artifacts):

- `build/wifi06_boot_20260818.bin`: 3324 bytes, SHA-256
  `56E4469CF6088F57066AD05DBCE5B3F174D6C8E4569B021EC97359C68179C32D`.
- `build/wifi06_connect_20260818.bin`: 235 bytes, SHA-256
  `3D940FACDB86A9074DEDDF7AA361EE3A3E070F75A840A5510C1F0F62905260C6`.

The local ESP-IDF tool registry is incomplete, so the successful build used explicit CMake, Ninja,
and Xtensa tool paths. Missing ROM ELF files produced a GDB-init warning only; image generation and
partition-size checks passed.

## Required hardware validation

- Boot log reports SoftAP start without reset loops or Wi-Fi/NVS errors.
- Phone and Windows client can associate and reach `http://192.168.4.1`.
- GET/POST accept valid boundaries and reject invalid/missing/oversized values.
- Refresh and power-cycle retain the last valid record.
- Controller rejection and NVS failure leave the previous active state recoverable.
- Long reconnect/configuration cycles do not grow heap or starve watchdogs.
- Audio/SD/amplifier validation is owned by a separate recipe and requires its own measurements.

## Optional later validation

- Captive-portal DNS and auto-popup behavior across Android, iOS, and Windows.
- Security review and per-device credential provisioning for product deployment.
