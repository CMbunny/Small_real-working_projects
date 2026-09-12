/*
 * http_ota_trigger.h — HTTP endpoint to trigger OTA from a direct URL
 *
 * One route: POST /api/ota/trigger  {"url": "https://.../firmware.bin"}
 *
 * No presigned-URL server, no API key, no firmware list. You host the
 * .bin wherever you want (your own server, a file share, S3 with a plain
 * public/pre-shared link) and just hand this endpoint a direct URL.
 *
 * This endpoint is UNAUTHENTICATED as written. That is almost certainly
 * wrong for your deployment — see the warning in http_ota_trigger.c
 * before you ship this on a network anyone else can reach. Flashing
 * arbitrary firmware onto a device is about as sensitive an action as
 * exists; do not expose this without at minimum a shared secret header
 * or your existing auth mechanism.
 */
#pragma once
#include "esp_http_server.h"

/* Register the /api/ota/trigger route on the given httpd handle. */
void http_ota_trigger_register_routes(httpd_handle_t server);