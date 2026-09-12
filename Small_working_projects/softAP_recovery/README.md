# ESP32 SoftAP Recovery / WiFi Provisioning

A minimal, self-contained WiFi recovery mechanism for ESP32 devices. When the
device has no working WiFi connection, it raises its own access point so a
technician can reconfigure WiFi from a phone — no laptop, no serial cable,
no prior network access required.

## What this actually does

1. Device can't connect to WiFi (no stored creds, or sustained connection
   failure).
2. Device raises a WPA2 access point: `Device-XXXXXX` (XXXXXX = last 3 bytes
   of the device's MAC address), password `RBXXXXXX` (same 3 bytes, hex).
3. Phone joins that AP, browses to `https://192.168.1.6:7443`, and sees a
   one-field form: SSID + password.
4. Technician enters the site's real WiFi credentials, taps Save, then Reboot.
5. Device reboots, reads the saved credentials from flash (NVS), and attempts
   to join that network normally.

No cloud dependency, no companion app, no BLE provisioning stack — just an
HTTP form served from the device itself.

## Files

| File | Purpose |
|---|---|
| `network_softap.h` / `.c` | Raises/tears down the AP itself, derives password from MAC, sets static IP `192.168.1.6` |
| `http_provision.h` / `.c` | Two HTTP endpoints: `POST /provision` (save creds), `POST /provision/boot` (apply + reboot) |
| `fallback_softap.html` | The page served to a phone connected to the AP |

## Requirements

- ESP-IDF (uses `esp_wifi`, `esp_netif`, `esp_http_server`, `nvs_flash`, `cJSON`)
- A component/CMakeLists entry that pulls in `nvs_flash`, `esp_http_server`,
  `json` (cJSON)

## Integration — what you have to wire yourself

This module does **not** manage your WiFi connection lifecycle. You need to:

1. **Call `wifi_start_softap_fallback()`** from wherever your project decides
   WiFi has failed (e.g. no stored SSID at boot, or N consecutive STA
   connection failures).
2. **Call `wifi_stop_softap_fallback()`** once STA successfully reconnects
   (on `IP_EVENT_STA_GOT_IP`).
3. **Read WiFi credentials from NVS** at boot, before calling
   `esp_wifi_connect()`:
```c
   nvs_handle_t h;
   char ssid[64] = {0}, pass[64] = {0};
   size_t ssid_len = sizeof(ssid), pass_len = sizeof(pass);
   if (nvs_open("wifi_cfg", NVS_READONLY, &h) == ESP_OK) {
       nvs_get_str(h, "ssid", ssid, &ssid_len);
       nvs_get_str(h, "pass", pass, &pass_len);
       nvs_close(h);
   }
```
4. **Register the provision routes** on your SoftAP-mode HTTP server only:
```c
   http_provision_register_routes(server);
```
5. **Check `network_softap_active()`** in your STA retry loop and stop (or
   drastically slow) reconnection attempts while it's true — see the
   single-radio warning below. This is not optional.

## Critical warning: single WiFi radio

The ESP32 has one radio. In APSTA mode (AP + STA both active), the AP's
beacon channel follows whatever channel STA is currently on. If your STA
retry/scan logic keeps running after the AP goes up, the radio hops channels
mid-handshake with any phone connected to the AP — the phone's TLS handshake
to the recovery page fails (confirmed on hardware: `mbedtls -0x7700` /
connection reset).

**Once `network_softap_active()` is true, your STA reconnect loop must stop
scanning entirely, or fall back to a multi-minute interval.** This library
does not own your retry loop; wiring this check in is your responsibility.

## Known constraint: static IP subnet

The AP is configured with static IP `192.168.1.6` (gateway) and a small DHCP
pool starting at `192.168.1.10`. This is a common home/office router subnet.
If this device's STA interface ever joins a router also on `192.168.1.x`,
you will get a routing conflict between the AP and STA interfaces on the
same chip. Change the subnet in `network_softap.c` (`softap_set_static_ip()`)
if your deployment risks this — e.g. `192.168.66.1`.

## Security notes

- The provisioning endpoints (`/provision`, `/provision/boot`) are
  **unauthenticated by design**. The trust boundary is physical: anyone
  hitting these endpoints is already connected to the device's own
  WPA2-protected AP.
- The AP password is derived from the device's MAC address, which is not a
  strong secret (visible on device labels, discoverable via mDNS/ARP). Do
  not extend these endpoints to accept anything beyond WiFi SSID/password —
  no MQTT credentials, no API keys, nothing that would matter if leaked.
- Never log the SoftAP password to a shared/multi-device log aggregator.

## Not included (by design)

This is a stripped-down extraction of a larger firmware's SoftAP feature.
Deliberately left out:

- SPIFFS/config-file recovery (restoring corrupted web UI files)
- LAN-mode WiFi editing after the device is already online
- Login/authentication flows for the recovery page
- Any config beyond SSID/password

If you need any of these, they require separate implementation.

## License

MIT — adapt as needed.
