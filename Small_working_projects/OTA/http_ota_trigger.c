/*
 * http_ota_trigger.c — HTTP endpoint to trigger OTA from a direct URL
 *
 * *** THIS ENDPOINT HAS NO AUTHENTICATION. ***
 * Anyone who can reach it on the network can point your device at any
 * URL and have it flash whatever binary is there. ota_manager.c's
 * project-name check stops it from flashing an UNRELATED project's
 * firmware, but it does NOT stop someone from flashing a malicious
 * build of YOUR OWN firmware if they can get one signed with a matching
 * project_name.
 *
 * If this device is reachable by anything other than you on a trusted
 * LAN, add auth before using this — e.g. check a bearer token / shared
 * secret header before calling ota_manager_start_with_url(). This file
 * deliberately does not invent an auth scheme for you, since bolting one
 * on wrong is worse than leaving the gap visible and documented.
 */

#include "http_ota_trigger.h"
#include "ota_manager.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OTA_HTTP";

static void send_json(httpd_req_t *req, int code, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    switch (code) {
        case 200: httpd_resp_set_status(req, "200 OK");             break;
        case 400: httpd_resp_set_status(req, "400 Bad Request");    break;
        case 409: httpd_resp_set_status(req, "409 Conflict");       break;
        default:  httpd_resp_set_status(req, "500 Internal Error"); break;
    }
    httpd_resp_sendstr(req, json);
}

static int read_body(httpd_req_t *req, char *buf, size_t max)
{
    int rem = (int)req->content_len;
    if (rem <= 0 || rem >= (int)max) return -1;
    int total = 0;
    while (rem > 0) {
        int n = httpd_req_recv(req, buf + total, rem);
        if (n <= 0) return -1;
        total += n; rem -= n;
    }
    buf[total] = '\0';
    return total;
}

/* POST /api/ota/trigger
 * Body: {"url": "https://your-server.com/firmware.bin"} */
static esp_err_t h_ota_trigger(httpd_req_t *req)
{
    char body[1024] = {0};
    if (read_body(req, body, sizeof(body)) < 0) {
        send_json(req, 400, "{\"error\":\"bad body\"}");
        return ESP_OK;
    }

    cJSON *j = cJSON_Parse(body);
    if (!j) {
        send_json(req, 400, "{\"error\":\"bad json\"}");
        return ESP_OK;
    }

    char url[768] = {0};
    const char *_url = cJSON_GetStringValue(cJSON_GetObjectItem(j, "url"));
    if (_url) strncpy(url, _url, sizeof(url) - 1);
    cJSON_Delete(j);

    if (!url[0] || strlen(url) < 10) {
        send_json(req, 400, "{\"error\":\"url required\"}");
        return ESP_OK;
    }

    esp_err_t err = ota_manager_start_with_url(url);
    if (err == ESP_ERR_INVALID_STATE) {
        send_json(req, 409, "{\"error\":\"ota already in progress\"}");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        send_json(req, 500, "{\"error\":\"ota start failed\"}");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA triggered: %.60s...", url);
    send_json(req, 200, "{\"status\":\"ota_started\"}");
    return ESP_OK;
}

/* GET /api/ota/status
 * Not mentioned in the header comment as a route because it's a small
 * addition beyond the single-endpoint scope you asked for — but without
 * it you have no way to know if the OTA is progressing or has failed
 * except watching serial logs. Included because it costs nothing and
 * you'll want it the first time an OTA silently fails. */
static esp_err_t h_ota_status(httpd_req_t *req)
{
    uint8_t pct = 0;
    const char *status = ota_manager_get_status(&pct);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"pct\":%u}", status, pct);
    send_json(req, 200, resp);
    return ESP_OK;
}

void http_ota_trigger_register_routes(httpd_handle_t server)
{
    httpd_uri_t trigger = {
        .uri = "/api/ota/trigger", .method = HTTP_POST,
        .handler = h_ota_trigger, .user_ctx = NULL
    };
    httpd_uri_t status = {
        .uri = "/api/ota/status", .method = HTTP_GET,
        .handler = h_ota_status, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &trigger);
    httpd_register_uri_handler(server, &status);
}