/*
 * http_provision.h — HTTP endpoints for SoftAP WiFi provisioning
 *
 * Two routes, meant to be registered on your HTTP server ONLY while
 * network_softap_active() is true (see network_softap.h):
 *
 *   POST /provision       -> save SSID + password (does not apply yet)
 *   POST /provision/boot  -> reboot device to apply saved creds
 *
 * These are deliberately UNAUTHENTICATED. The trust boundary is physical:
 * whoever can reach this endpoint is already connected to the device's
 * own WPA2-protected AP. Do not add any other config surface to these
 * handlers — MQTT/API credentials must never travel over the SoftAP path,
 * since the AP password is derivable from the device's MAC (see
 * network_softap.c) and is not a strong secret.
 *
 * Depends on network_softap.h for network_softap_active() gating.
 */
#pragma once
#include "esp_http_server.h"

/* Register both routes on the given httpd handle. Call this only when
 * setting up your SoftAP-mode HTTP server, not your normal LAN server. */
void http_provision_register_routes(httpd_handle_t server);