# Transport Context: ESP32 SoftAP + HTTP

## Ownership

- ESP32 native 2.4 GHz Wi-Fi operates as SoftAP plus an optional station uplink.
- ESP-IDF owns radio, TCP/IP, DHCP, and HTTP server resources.
- Default endpoint is `http://192.168.4.1`; Internet access is not required.
- The portal component exclusively owns the Wi-Fi driver while it is running.
- STA may be suspended while SoftAP/HTTP remain active and restored later by the controller.

## Contract and bounds

- `GET /` serves the embedded configuration page.
- `/api/v1/alarm/0` and `/api/v1/status` expose the single debug schedule and runtime state.
- `/api/v1/network/status` never returns a password.
- `/api/v1/network/config` accepts strict bounded JSON and uses asynchronous connect-before-save.
- `/api/v1/network/scan` returns at most ten nearby SSIDs, RSSI values and auth modes.
- HTTP request bodies are bounded to 384 bytes; SSID is 1..32 bytes and password is empty or 8..63
  printable ASCII bytes.
- WPA2-PSK is used when a non-empty 8-63 byte password is configured.

## Exclusions

- No cloud service, MQTT, TLS, HTTP authentication, or guarantee that a client OS opens the page.
- Wildcard DNS redirects to the local portal, but captive-page behavior remains OS-dependent.
- No SD-card or audio data travels through this transport in version 1.

## Credential persistence

- `alarm_net/active` contains magic, version, configured flag, SSID/password and checksum.
- Candidate credentials remain RAM-only until the STA obtains an IP and NVS commit succeeds.
- Connection or commit failure restores the previous runtime network.
- Clearing writes an explicit unconfigured record; it does not erase the key and fall back to a
  compile-time credential on reboot.
- Shared SoftAP credentials and plaintext local HTTP make this transport `development_only`.
