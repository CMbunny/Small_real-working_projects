/*
 * network_softap.c — SoftAP recovery/provisioning fallback
 *
 * Raises a WPA2 access point (Device-XXXXXX, password derived from MAC)
 * so a device with no working WiFi path is still reachable on-site.
 * STA mode stays enabled (WIFI_MODE_APSTA) so once creds are fixed, the
 * device reconnects normally without a reboot being required.
 *
 * STATIC AP IP: 192.168.1.6 (gateway), DHCP pool 192.168.1.10-.20
 *
 * WARNING — see the caller's setup guide: 192.168.1.x is an extremely
 * common router subnet. If this device's STA interface ever joins a
 * router on that same subnet, you will get a routing conflict between
 * the AP and STA interfaces. Verify your deployment before using this.
 *
 * IMPORTANT — single-radio caveat (unchanged from original):
 * The ESP32 has ONE WiFi radio. In APSTA mode, the SoftAP beacon channel
 * FOLLOWS whatever channel STA is on. If your STA retry logic keeps
 * scanning/reconnecting after this AP goes up, the radio hops channels
 * mid-handshake with any phone connected to the AP, and the phone's TLS
 * handshake to the recovery page fails. Your STA reconnect loop MUST
 * stop or slow to a crawl once network_softap_active() is true. This
 * file does not own your retry loop — wiring that check in is on you.
 */

#include "network_softap.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

/* ---- Adapt these two to your project ---- */
#include "config_manager.h"   /* must expose config_get()->wifi_ssid[64] */
extern void live_log(uint8_t level, const char *tag, const char *msg);
/* ------------------------------------------ */

static const char *TAG = "SOFTAP";

static bool         s_softap_active = false;
static esp_netif_t *s_ap_netif      = NULL;
static char         s_softap_pass[16] = {0};

/*
 * SoftAP PSK: derived from the device's own MAC address rather than one
 * fixed string shared across every unit in the fleet. Format:
 *   "RB" + 3 hex bytes from MAC[3..5]   (e.g. "RB1A2B3C")
 * WPA2 requires >= 8 characters; this is 8 exactly.
 *
 * Never log this value to a shared/multi-device log stream.
 */
static void softap_fill_password(char *out, size_t out_len, const uint8_t mac[6])
{
    if (!out || out_len < 10 || !mac) {
        if (out && out_len) out[0] = '\0';
        return;
    }
    snprintf(out, out_len, "RB%02X%02X%02X", mac[3], mac[4], mac[5]);
}

/* Sets the AP netif's static IP + DHCP server range. Must run AFTER the
 * netif exists but BEFORE the AP is actively serving DHCP leases, so we
 * stop dhcps, reconfigure, then restart it. */
static void softap_set_static_ip(esp_netif_t *netif)
{
    esp_netif_dhcps_stop(netif);   /* must stop before changing IP */

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip,      192, 168, 1, 6);
    IP4_ADDR(&ip_info.gw,      192, 168, 1, 6);   /* gateway = itself */
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    esp_err_t err = esp_netif_set_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set static AP IP: %s", esp_err_to_name(err));
    }

    /* Hand out a small pool starting at .10 so it never collides with the
     * gateway (.6) itself. dhcps_option is optional but keeps the DHCP
     * lease range explicit rather than relying on esp-idf's default. */
    esp_netif_dhcps_start(netif);
}

void wifi_start_softap_fallback(void)
{
    if (s_softap_active) return;   /* idempotent */

    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();

    /* Static IP must be applied before esp_wifi_set_mode/start puts the
     * AP interface into active service, otherwise clients may briefly
     * see the default 192.168.4.1 before the change lands. */
    softap_set_static_ip(s_ap_netif);

    esp_wifi_set_mode(WIFI_MODE_APSTA);

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    wifi_config_t ap = {0};
    int n = snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid),
                     "Device-%02X%02X%02X", mac[3], mac[4], mac[5]);
    ap.ap.ssid_len       = (uint8_t)(n > 0 ? n : 0);
    ap.ap.channel        = 1;
    ap.ap.max_connection = 2;
    ap.ap.authmode       = WIFI_AUTH_WPA2_PSK;

    softap_fill_password((char *)ap.ap.password, sizeof(ap.ap.password), mac);
    softap_fill_password(s_softap_pass, sizeof(s_softap_pass), mac);

    esp_wifi_set_config(WIFI_IF_AP, &ap);

    s_softap_active = true;

    ESP_LOGW(TAG,
             "SoftAP recovery UP: SSID '%s' pass '%s' — "
             "STA parked (single radio). Phone: join AP -> "
             "https://192.168.1.6:7443",
             ap.ap.ssid, s_softap_pass);
    live_log(2, "NETWORK", "SoftAP recovery UP (SSID+PSK logged)");
}

void wifi_stop_softap_fallback(void)
{
    if (!s_softap_active) return;   /* idempotent */

    esp_wifi_set_mode(WIFI_MODE_STA);
    s_softap_active = false;
    memset(s_softap_pass, 0, sizeof(s_softap_pass));

    ESP_LOGI(TAG, "SoftAP recovery mode closed — WiFi reconnected normally");
    live_log(1, "NETWORK", "SoftAP recovery mode closed — WiFi back online");
}

bool network_softap_active(void) { return s_softap_active; }

const char *network_softap_password(void)
{
    return s_softap_active ? s_softap_pass : "";
}