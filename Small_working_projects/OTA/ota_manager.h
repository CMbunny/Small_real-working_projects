/*
 * ota_manager.h — Minimal HTTPS OTA firmware updater
 *
 * Downloads a firmware .bin from a direct URL over whatever network
 * interface is currently up (WiFi or Ethernet — this code is interface-
 * agnostic; it just uses esp_http_client, which works over any active
 * esp_netif). Flashes it to the inactive OTA partition and reboots.
 *
 * SAFETY MECHANISM — READ THIS BEFORE YOU SKIP IT:
 * ESP-IDF's OTA rollback feature means a freshly-flashed image boots in
 * a "PENDING_VERIFY" state. If nothing explicitly marks it valid, and the
 * device reboots or crashes before that happens, the BOOTLOADER ITSELF
 * reverts to the previous known-good image automatically. This is the
 * only thing standing between "bad firmware" and "permanently bricked
 * device." You MUST call:
 *   - ota_manager_check_state() early in app_main(), before anything
 *     that could crash
 *   - ota_manager_confirm_pending_valid() later, only after the new
 *     firmware has run stably for some minutes (your choice how long)
 * Skipping either of these means every OTA is a bet against a reboot.
 *
 * Requirements (add to your component's CMakeLists.txt REQUIRES):
 *   esp_https_ota, esp_http_client, app_update, esp_https_ota,
 *   esp_http_server (only if you also use http_ota_trigger.c)
 *
 * Requires a partition table with at least ota_0 + ota_1 + otadata —
 * see partitions.csv in this same folder for a working reference, and
 * sdkconfig.defaults must have CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y.
 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>

/*
 * Call once, early in app_main(), BEFORE any code that could crash or
 * hang. Checks whether this boot is the first boot after an OTA flash
 * (partition state == PENDING_VERIFY). If so, arms the "needs
 * confirmation" flag — see ota_manager_confirm_pending_valid().
 * Cheap and side-effect-free if this is a normal boot (not post-OTA).
 */
void ota_manager_check_state(void);

/*
 * Call from your main loop, ONLY after the device has been running
 * stably for some period you're comfortable with (the reference
 * implementation used ~5 minutes of uptime). Marks the currently
 * running image as permanently valid and cancels the pending rollback.
 * No-op if this boot was not a post-OTA boot (i.e. ota_manager_check_state
 * found nothing pending) — safe to call unconditionally from your idle
 * loop on every boot.
 */
void ota_manager_confirm_pending_valid(void);

/*
 * Starts an OTA update from a direct HTTPS URL to a .bin file.
 * Spawns its own FreeRTOS task and returns immediately — this call is
 * non-blocking. Progress can be observed via ota_manager_get_status()
 * or (if you use http_ota_trigger.c) polled over the same HTTP endpoint.
 *
 * Refuses (returns ESP_ERR_INVALID_STATE) if an OTA is already running.
 * Refuses (returns ESP_ERR_INVALID_ARG) if url is NULL or obviously too
 * short to be real.
 *
 * Safety checks performed automatically before flashing:
 *   - HTTP status must be 200 and Content-Length must fit the partition
 *   - the downloaded image's embedded project_name must match this
 *     firmware's own project_name (won't flash an unrelated .bin)
 *   - if the image's version string matches the currently running
 *     version exactly, the flash is skipped (nothing to do)
 */
esp_err_t ota_manager_start_with_url(const char *url);

/* Human-readable current status: "idle", "downloading", "flashing",
 * "verifying", "success_rebooting", or an error string. Progress
 * percentage (0-100) is written to *pct_out if non-NULL. Thread-safe. */
const char *ota_manager_get_status(uint8_t *pct_out);

/* True while an OTA is actively in progress (download/flash/verify). */
bool ota_manager_in_progress(void);