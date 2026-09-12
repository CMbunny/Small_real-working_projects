/*
 * ota_manager.c — Minimal HTTPS OTA firmware updater
 *
 * See ota_manager.h for the safety-mechanism explanation (rollback /
 * PENDING_VERIFY). Do not skip wiring that in.
 */

#include "ota_manager.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#define TAG "OTA_MGR"

/* R2/S3/most presigned or direct URLs can run long; 600 chars is generous
 * headroom over a typical bucket URL with a query string. */
#define OTA_URL_MAX 768

typedef struct { char url[OTA_URL_MAX]; } ota_args_t;

/* -- Shared state, protected by a mutex since ota_manager_get_status()
 *    can be called from an HTTP handler task while the OTA task itself
 *    is writing to these same fields. -- */
static SemaphoreHandle_t s_status_mutex   = NULL;
static bool              s_ota_running    = false;
static char              s_status[32]     = "idle";
static uint8_t           s_progress_pct   = 0;

static bool s_awaiting_rollback_confirm = false;

static void set_status(const char *status, uint8_t pct)
{
    if (!s_status_mutex) return;
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    strncpy(s_status, status, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    s_progress_pct = pct;
    xSemaphoreGive(s_status_mutex);
}

const char *ota_manager_get_status(uint8_t *pct_out)
{
    static char out[32];
    if (!s_status_mutex) {
        if (pct_out) *pct_out = 0;
        return "idle";
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    strncpy(out, s_status, sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';
    if (pct_out) *pct_out = s_progress_pct;
    xSemaphoreGive(s_status_mutex);
    return out;
}

bool ota_manager_in_progress(void)
{
    if (!s_status_mutex) return false;
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    bool running = s_ota_running;
    xSemaphoreGive(s_status_mutex);
    return running;
}

/* ================================================================
 * Rollback safety net (see ota_manager.h for why this matters)
 * ================================================================ */
void ota_manager_check_state(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return;

    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        /* Deliberately do NOT mark valid here. If we did, a firmware that
         * merely boots but is otherwise broken (crash-loops a minute
         * later, can't do its actual job) would be confirmed permanently
         * and rollback would never trigger. The whole point of PENDING_VERIFY
         * is "prove you're stable before I commit to you." */
        s_awaiting_rollback_confirm = true;
        ESP_LOGW(TAG, "New firmware on probation — awaiting stability confirmation");
    }
}

void ota_manager_confirm_pending_valid(void)
{
    if (!s_awaiting_rollback_confirm) return;   /* not a post-OTA boot */

    esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
    if (e == ESP_OK) {
        s_awaiting_rollback_confirm = false;
        ESP_LOGI(TAG, "Firmware confirmed stable — rollback cancelled");
    } else {
        ESP_LOGE(TAG, "mark_app_valid failed: %s — will retry next call",
                 esp_err_to_name(e));
    }
}

/* ================================================================
 * Version check — skip re-flashing an identical image
 * ================================================================ */
static bool version_is_new(const esp_app_desc_t *new_app)
{
    const esp_app_desc_t *running = esp_app_get_description();
    if (!running || !new_app) return true;
    ESP_LOGI(TAG, "Running: [%s]  Incoming: [%s]",
             running->version, new_app->version);
    if (strcmp(new_app->version, running->version) == 0) {
        ESP_LOGW(TAG, "Same version already installed — skipping");
        return false;
    }
    return true;
}

/* ================================================================
 * OTA worker task — the actual download/flash/verify/reboot cycle
 * ================================================================ */
static void ota_task(void *arg)
{
    ota_args_t *args = (ota_args_t *)arg;
    esp_https_ota_handle_t handle = NULL;
    bool handle_open = false;

    ESP_LOGW(TAG, "OTA started");
    set_status("connecting", 0);

    if (!args->url[0] || strlen(args->url) < 10) {
        set_status("failed_bad_url", 0);
        ESP_LOGE(TAG, "Invalid firmware URL");
        goto done;
    }

    esp_http_client_config_t http_cfg = {
        .url                          = args->url,
        .timeout_ms                   = 20000,
        .keep_alive_enable            = false,
        .skip_cert_common_name_check  = false,
        .crt_bundle_attach            = esp_crt_bundle_attach,
        .buffer_size                  = 4096,
        .buffer_size_tx               = 4096,
    };
    esp_https_ota_config_t ota_cfg = { .http_config = &http_cfg };

    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK || !handle) {
        ESP_LOGE(TAG, "OTA begin failed: %s (0x%x)",
                 esp_err_to_name(err), (unsigned)err);
        set_status("failed_begin", 0);
        goto done;
    }
    handle_open = true;

    /* Refuse to flash an image built for a different project. This is a
     * real guard, not paranoia — a wrong URL pointing at someone else's
     * firmware would otherwise brick the device silently. */
    {
        esp_app_desc_t new_desc = {0};
        if (esp_https_ota_get_img_desc(handle, &new_desc) == ESP_OK) {
            const esp_app_desc_t *self = esp_app_get_description();
            if (self && strncmp(new_desc.project_name, self->project_name,
                                sizeof(new_desc.project_name)) != 0) {
                ESP_LOGE(TAG,
                         "REFUSED — image is \"%.31s\", this firmware is \"%.31s\"",
                         new_desc.project_name, self->project_name);
                esp_https_ota_abort(handle);
                handle_open = false;
                set_status("failed_wrong_project", 0);
                goto done;
            }
            if (!version_is_new(&new_desc)) {
                esp_https_ota_abort(handle);
                handle_open = false;
                set_status("skipped_same_version", 0);
                goto done;
            }
        }
    }

    set_status("downloading", 0);

    {
        int total_sz   = esp_https_ota_get_image_size(handle);
        uint8_t last_pct = 0;

        while (true) {
            err = esp_https_ota_perform(handle);
            if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                int written = esp_https_ota_get_image_len_read(handle);
                uint8_t pct = (total_sz > 0)
                    ? (uint8_t)(((uint64_t)written * 100ULL) / (uint64_t)total_sz)
                    : 0;
                if (pct != last_pct) {
                    last_pct = pct;
                    set_status("downloading", pct);
                    ESP_LOGI(TAG, "Downloading %u%%", (unsigned)pct);
                }
                continue;
            }
            break;
        }

        if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
            esp_https_ota_abort(handle);
            handle_open = false;
            set_status("failed_download", last_pct);
            ESP_LOGE(TAG, "Download incomplete or connection lost");
            goto done;
        }
    }

    set_status("flashing", 0);
    err = esp_https_ota_finish(handle);
    handle_open = false;   /* finish() consumes the handle regardless of result */

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Flash write/verify failed: %s (0x%x)",
                 esp_err_to_name(err), (unsigned)err);
        set_status("failed_finish", 0);
        goto done;
    }

    set_status("success_rebooting", 100);
    ESP_LOGW(TAG, "OTA success — rebooting in 2s");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    /* not reached */

done:
    if (handle_open && handle) {
        esp_https_ota_abort(handle);
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    s_ota_running = false;
    xSemaphoreGive(s_status_mutex);
    free(args);
    vTaskDelete(NULL);
}

esp_err_t ota_manager_start_with_url(const char *url)
{
    if (!url || strlen(url) < 10) return ESP_ERR_INVALID_ARG;

    if (!s_status_mutex) s_status_mutex = xSemaphoreCreateMutex();

    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    if (s_ota_running) {
        xSemaphoreGive(s_status_mutex);
        ESP_LOGW(TAG, "OTA already in progress — ignoring new request");
        return ESP_ERR_INVALID_STATE;
    }
    s_ota_running = true;
    xSemaphoreGive(s_status_mutex);

    ota_args_t *args = malloc(sizeof(ota_args_t));
    if (!args) {
        xSemaphoreTake(s_status_mutex, portMAX_DELAY);
        s_ota_running = false;
        xSemaphoreGive(s_status_mutex);
        return ESP_ERR_NO_MEM;
    }
    strncpy(args->url, url, sizeof(args->url) - 1);
    args->url[sizeof(args->url) - 1] = '\0';

    if (xTaskCreate(ota_task, "ota_task", 8192, args, 5, NULL) != pdPASS) {
        free(args);
        xSemaphoreTake(s_status_mutex, portMAX_DELAY);
        s_ota_running = false;
        xSemaphoreGive(s_status_mutex);
        ESP_LOGE(TAG, "OTA task create failed — OOM");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}