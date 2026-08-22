# Wi-Fi alarm portal

ESP32 native SoftAP + optional station uplink + local HTTP configuration service for alarm-car integration.

Owned here:

- SoftAP lifecycle and local page at `http://192.168.4.1`
- optional APSTA lifecycle, bounded station retries, and IP-ready wait API
- runtime STA suspend/resume while keeping SoftAP and HTTP available
- nearby-network scanning and user-entered STA credentials through the local page
- two-phase STA provisioning: obtain an IP first, then commit credentials to NVS
- rollback to the previous runtime network if connection or NVS persistence fails
- wildcard DNS and OS connectivity-probe redirects for captive-portal discovery
- bounded validation of schedule ID, time, weekday mask, repeat, and enabled state
- versioned NVS persistence in namespace `alarm06`
- non-blocking event/status callbacks into an alarm controller
- strict JSON API for time sync, schedule update, status, and stopping one ringing event

Not owned here:

- SNTP/current-time acceptance, RTC maintenance, and alarm scheduling
- SD-card filesystem, audio decoding, I2S, amplifier control, or speaker safety
- device-specific guarantees that every phone OS version will auto-open the page

The component exclusively owns the ESP-IDF Wi-Fi driver while running. An empty station SSID keeps
APSTA available for web-initiated scanning but leaves station connection disabled. Station credentials are caller-owned configuration and are never
logged by this component. Callbacks run in the HTTP
server task and must be bounded, non-blocking, and must not re-enter the portal API.

The persisted `alarm06/config` record is versioned. Existing v1 records are migrated to v2 without
erasing NVS. The current debug contract supports `schedule_id=0` and uses `bit0=Monday` through
`bit6=Sunday`.

Station configuration uses the separate `alarm_net/active` NVS record. The record has a magic,
version, explicit configured flag, and checksum. A successful web update is not persisted until the
station receives an IP address. Clearing the setting persists an explicit unconfigured record, so a
restart does not silently restore build-time credentials. Passwords are never returned by an API or
written to logs.

Network endpoints:

| Method | Path | Contract |
|---|---|---|
| `GET` | `/api/v1/network/status` | Configured SSID, link/provision state, result and disconnect reason; never the password |
| `PUT` | `/api/v1/network/config` | Strict bounded JSON `{ssid,password}`; starts asynchronous validation |
| `DELETE` | `/api/v1/network/config` | Switches to unconfigured state and persists the tombstone, with rollback on failure |
| `POST` | `/api/v1/network/scan` | Starts one bounded nearby-network scan |
| `GET` | `/api/v1/network/scan` | Returns at most ten SSIDs with RSSI and authentication mode |

The SoftAP uses a shared demonstration password and the HTTP service has no user authentication or
TLS. This capability is `development_only`; product release requires per-device provisioning,
authorization, credential-at-rest policy, malformed-input/fault tests, and a factory-reset contract.
