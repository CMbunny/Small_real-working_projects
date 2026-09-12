/*
 * network_softap.h — SoftAP recovery/provisioning fallback
 *
 * When the device has no usable WiFi credentials (or cannot reach a
 * configured AP for an extended period), it raises its own WPA2 access
 * point so a technician can reconfigure WiFi from a phone with no laptop,
 * no cable, and no prior network access.
 *
 * Dependencies this module assumes exist elsewhere in your project:
 *   - config_get() -> returns a struct with at least wifi_ssid[64]
 *   - live_log(level, tag, msg) -> your event logger (stub if you don't have one)
 *   - esp_wifi / esp_netif (ESP-IDF WiFi driver)
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Call once, on every WiFi disconnect / retry-timeout event, or immediately
 * at boot if there are no stored WiFi credentials at all. Idempotent —
 * safe to call repeatedly; it no-ops if the AP is already up. */
void wifi_start_softap_fallback(void);

/* Call once WiFi has successfully reconnected (got an IP). Drops back to
 * STA-only mode. Idempotent — no-ops if SoftAP is not currently active. */
void wifi_stop_softap_fallback(void);

/* True while the recovery AP is up. Used by:
 *   - the HTTP layer, to decide whether to mask secrets in config responses
 *   - the WiFi retry state machine, to know it must stop retrying STA
 *   - the recovery HTML page, to know which UI to show
 */
bool network_softap_active(void);

/* Returns the current SoftAP WPA2 password, or "" if SoftAP is not active.
 * Never log this value in a shared/multi-tenant log stream — see the .c
 * file for why it's derived from MAC rather than a single fleet-wide string. */
const char *network_softap_password(void);