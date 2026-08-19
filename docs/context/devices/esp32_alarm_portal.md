# Device Context: ESP32 alarm portal

## Target

- Board family: company ESP32-WROOM-32E-N4 board, classic ESP32, 4 MB flash.
- USB-UART: CH340; task 06 currently assigns COM22 based on enumeration evidence.
- Wi-Fi uses the module's native radio and does not introduce GPIO bindings.

## Device responsibility

- Start/stop SoftAP and local HTTP server.
- Load and persist a versioned alarm record in NVS namespace `alarm06`.
- Validate time and enabled state before handing them to the controller callback.
- Leave the last valid alarm unchanged when validation or controller application fails.

## Resource and safety rules

- Do not erase NVS automatically when initialization fails.
- Do not alter GPIO0, EN, bootloader, partition table, or download behavior.
- The callback is bounded and non-blocking; it may copy to a controller queue/state only.
- Wi-Fi failure does not directly start audio or modify amplifier/SD resources.
