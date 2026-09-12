# ESP32 OTA (Over-The-Air) Firmware Updater

A minimal, self-contained HTTPS OTA mechanism for ESP32-S3 devices. Point
the device at a URL to a `.bin` firmware file, and it downloads, verifies,
flashes, and reboots into it — with automatic rollback if the new firmware
turns out to be broken.

## What this actually does

1. You host a compiled firmware `.bin` somewhere reachable from the device
   (your own server, a file share, S3 with a plain link — anywhere serving
   plain HTTPS).
2. You send the device a URL: `POST /api/ota/trigger {"url": "https://.../firmware.bin"}`
3. Device downloads the file over HTTPS, in chunks, tracking progress.
4. Before flashing, it checks two things automatically:
   - The image's embedded project name matches this firmware's own name
     (refuses to flash an unrelated project's binary)
   - The image's version string differs from what's currently running
     (skips re-flashing an identical build)
5. Flashes the image to the *inactive* OTA partition (never overwrites the
   currently-running one — that's the whole point of having two).
6. Reboots into the new firmware.
7. New firmware boots in a "PENDING_VERIFY" state. If it doesn't get
   explicitly confirmed as stable, and the device reboots or crashes again,
   **the bootloader itself reverts to the previous working image automatically.**

This works over WiFi or Ethernet — the code doesn't care which. It just uses
`esp_http_client`, which operates over whatever network interface currently
has an IP address. No interface-specific code exists in this OTA layer.

## Files

| File | Purpose |
|---|---|
| `ota_manager.h` / `.c` | Core logic: download, project/version checks, flash, rollback confirmation |
| `http_ota_trigger.h` / `.c` | Two HTTP routes: trigger an OTA, check its status |
| `partitions.csv` | Partition table with the `ota_0`/`ota_1`/`otadata` layout OTA requires |
| `sdkconfig.defaults` | Build config: partition table, rollback enable, TLS, PSRAM |

## The rollback safety net — the part that actually matters

This is not a nice-to-have, it's the difference between "OTA update" and
"remote brick machine." Two functions handle it:

- **`ota_manager_check_state()`** — call this early in `app_main()`, before
  anything that could crash. Checks if this boot is the first boot after a
  flash. If so, arms a flag: this image is "on probation."

- **`ota_manager_confirm_pending_valid()`** — call this from your main/idle
  loop, but only after the device has run stably for some period you choose
  (a few minutes is reasonable). This tells the bootloader "this image is
  good, stop watching it." Safe to call unconditionally every loop iteration
  — it's a no-op if there's nothing pending.

**If you never call the confirm function, or the new firmware crashes before
you do, the bootloader automatically reverts to the last known-good image on
the next boot.** This requires `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` in
your `sdkconfig` (already set in `sdkconfig.defaults`) — without that one
line, this entire safety mechanism silently does nothing.

## Usage

```bash
# Trigger an update
curl -X POST http://<device-ip>/api/ota/trigger \
  -H "Content-Type: application/json" \
  -d '{"url":"https://your-server.com/firmware.bin"}'

# Check progress
curl http://<device-ip>/api/ota/status
# {"status":"downloading","pct":47}
```

Status values you'll see: `idle`, `connecting`, `downloading`, `flashing`,
`success_rebooting`, or a `failed_*` variant (`failed_bad_url`,
`failed_begin`, `failed_download`, `failed_finish`, `failed_wrong_project`,
`skipped_same_version`).

## Integration — what you have to wire yourself

This module does not run itself. Required in your `app_main()`:

```c
void app_main(void) {
    // ... your other init (NVS, WiFi, etc.) ...

    ota_manager_check_state();   // MUST run before anything that can crash

    // ... start your HTTP server ...
    http_ota_trigger_register_routes(server);

    // ... your main/idle loop ...
    while (1) {
        // after some stable uptime (your choice, e.g. 5 minutes):
        ota_manager_confirm_pending_valid();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## Partition table requirements

OTA needs a specific partition layout — a default single-app partition
table has no room for a second firmware slot and OTA cannot work at all.
`partitions.csv` provides:

- `ota_0` / `ota_1` — two equal-sized app slots (the device flashes into
  whichever one *isn't* currently running)
- `otadata` — tiny partition tracking which slot is active and whether it's
  pending verification

**Check `ota_0`/`ota_1` are large enough for your actual compiled firmware.**
Run `idf.py size` after building and compare against the partition size in
`partitions.csv`. If your firmware is bigger than the partition, the build
fails with a clear error — it won't silently corrupt anything, but you need
to enlarge both `ota_0` and `ota_1` (must stay equal size) in that case.

## Security warning — read before exposing this on any untrusted network

`/api/ota/trigger` has **no authentication**. As written, anyone who can
reach this endpoint over the network can tell your device to flash whatever
binary lives at a URL they control. The project-name check in
`ota_manager.c` stops it from flashing an *unrelated* project's firmware,
but does **not** stop someone from serving a malicious build of your own
firmware if they can get the project name to match.

If this device is reachable by anything other than you, on a network you
don't fully control, add authentication before using this — a bearer token
or shared-secret header check at the top of `h_ota_trigger()` in
`http_ota_trigger.c` is the minimum. This file deliberately does not invent
an auth scheme, since a wrong one is worse than an honestly-documented gap.

## Not included (by design)

This is a minimal extraction — explicitly scoped down from a larger
firmware's OTA feature. Left out on request:

- MQTT-triggered OTA (a second trigger path via broker subscription)
- SPIFFS/web-UI-file OTA (a separate mechanism for updating filesystem
  contents, distinct from firmware OTA)
- Presigned-URL / cloud-backend integration (Render, S3 signing, API keys)
  — you provide a direct URL instead
- Watchdog task registration during download — a stalled download here
  relies on `esp_http_client`'s own 20-second read timeout to eventually
  fail, rather than a hardware watchdog panic-reboot

If any of these become relevant later, they need separate implementation.

## License

MIT — adapt as needed.
