/*
 * http_provision.c — HTTP endpoints for SoftAP WiFi provisioning
 *
 * Storage: writes directly to NVS namespace "wifi_cfg", keys "ssid"/"pass".
 * This is intentionally NOT wired into a larger config-management system —
 * see the header comment. If your project has its own config layer, replace
 * the two nvs_set_str() calls in h_provision() with your own setter, and
 * make sure your boot-time WiFi init reads from the same place.
 */

#include "http_provision.h"
#include "network_softap.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "PROVISION";

#define NVS_NS_WIFI    "wifi_cfg"
#define NVS_KEY_SSID   "ssid"
#define NVS_KEY_PASS   "pass"

static void send_json(httpd_req_t *req, int code, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    switch (code) {
        case 200: httpd_resp_set_status(req, "200 OK");             break;
        case 400: httpd_resp_set_status(req, "400 Bad Request");    break;
        case 403: httpd_resp_set_status(req, "403 Forbidden");      break;
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

/* POST /provision
 * Body: {"ssid":"...","password":"..."}
 * Gated: only accepted while the recovery AP is actually up. This stops
 * someone hitting this endpoint over the normal LAN HMI and silently
 * rewriting WiFi creds through a path that was never meant for that. */
static esp_err_t h_provision(httpd_req_t *req)
{
    if (!network_softap_active()) {
        send_json(req, 403, "{\"error\":\"not in recovery mode\"}");
        return ESP_OK;
    }

    char body[512] = {0};
    if (read_body(req, body, sizeof(body)) < 0) {
        send_json(req, 400, "{\"error\":\"bad body\"}");
        return ESP_OK;
    }

    cJSON *in = cJSON_Parse(body);
    if (!in) {
        send_json(req, 400, "{\"error\":\"bad json\"}");
        return ESP_OK;
    }

    cJSON *jssid = cJSON_GetObjectItem(in, "ssid");
    cJSON *jpass = cJSON_GetObjectItem(in, "password");
    if (!cJSON_IsString(jssid) || !jssid->valuestring || !jssid->valuestring[0]) {
        cJSON_Delete(in);
        send_json(req, 400, "{\"error\":\"ssid required\"}");
        return ESP_OK;
    }

    const char *ssid = jssid->valuestring;
    const char *pass = (cJSON_IsString(jpass) && jpass->valuestring)
                       ? jpass->valuestring : "";

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        cJSON_Delete(in);
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        send_json(req, 500, "{\"error\":\"storage unavailable\"}");
        return ESP_OK;
    }

    nvs_set_str(h, NVS_KEY_SSID, ssid);
    nvs_set_str(h, NVS_KEY_PASS, pass);
    esp_err_t commit_err = nvs_commit(h);
    nvs_close(h);
    cJSON_Delete(in);

    if (commit_err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(commit_err));
        send_json(req, 500, "{\"error\":\"save failed\"}");
        return ESP_OK;
    }

    /* Deliberately does NOT reboot here — saving and applying are separate
     * steps, same as pressing a physical "Enable" button vs a "Boot" button.
     * Call /provision/boot to actually apply. */
    send_json(req, 200, "{\"status\":\"saved\"}");
    ESP_LOGI(TAG, "WiFi creds saved via SoftAP — waiting for /provision/boot");
    return ESP_OK;
}

/* POST /provision/boot
 * Reboots the device so it re-reads WiFi creds from NVS and attempts STA.
 * Same gating as h_provision() — only works while SoftAP is active. */
static esp_err_t h_provision_boot(httpd_req_t *req)
{
    if (!network_softap_active()) {
        send_json(req, 403, "{\"error\":\"not in recovery mode\"}");
        return ESP_OK;
    }

    send_json(req, 200, "{\"status\":\"rebooting\"}");
    ESP_LOGW(TAG, "Reboot requested via SoftAP provisioning");

    vTaskDelay(pdMS_TO_TICKS(1200));   /* let the HTTP response flush first */
    esp_restart();
    return ESP_OK;   /* not reached */
}

void http_provision_register_routes(httpd_handle_t server)
{
    httpd_uri_t prov = {
        .uri = "/provision", .method = HTTP_POST,
        .handler = h_provision, .user_ctx = NULL
    };
    httpd_uri_t prov_boot = {
        .uri = "/provision/boot", .method = HTTP_POST,
        .handler = h_provision_boot, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &prov);
    httpd_register_uri_handler(server, &prov_boot);
}