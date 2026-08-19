# Transport Context: ESP32 SoftAP + HTTP

## Ownership

- ESP32 native 2.4 GHz Wi-Fi operates as a SoftAP.
- ESP-IDF owns radio, TCP/IP, DHCP, and HTTP server resources.
- Default endpoint is `http://192.168.4.1`; Internet access is not required.
- The portal component exclusively owns the Wi-Fi driver while it is running.

## Contract and bounds

- `GET /` serves the embedded configuration page.
- `GET /api/alarm` returns the active `hour`, `minute`, and `enabled` values.
- `POST /api/alarm` accepts bounded form data; body length is at most 96 bytes.
- Hour is `0..23`, minute is `0..59`, and enabled is `0|1`.
- WPA2-PSK is used when a non-empty 8-63 byte password is configured.

## Exclusions

- No STA connection, cloud service, MQTT, NTP, TLS, or Internet dependency.
- No DNS captive portal or guarantee that a client OS automatically opens the page.
- No SD-card or audio data travels through this transport in version 1.
