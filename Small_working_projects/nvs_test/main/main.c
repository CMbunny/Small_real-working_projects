#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "NVS_TEST"
#define NVS_NS "test_ns"

typedef struct {
    char name[16];
    int  value;
} test_blob_t;

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &h));

    test_blob_t write_data = { .name = "hello", .value = 42 };
    ESP_ERROR_CHECK(nvs_set_blob(h, "blob_key", &write_data, sizeof(write_data)));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI(TAG, "Wrote: name=%s value=%d", write_data.name, write_data.value);

    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READONLY, &h));
    test_blob_t read_data = {0};
    size_t sz = sizeof(read_data);
    ESP_ERROR_CHECK(nvs_get_blob(h, "blob_key", &read_data, &sz));
    nvs_close(h);
    ESP_LOGI(TAG, "Read:  name=%s value=%d", read_data.name, read_data.value);

    if (memcmp(&write_data, &read_data, sizeof(test_blob_t)) == 0) {
        ESP_LOGI(TAG, "PASS: data matches");
    } else {
        ESP_LOGE(TAG, "FAIL: data mismatch");
    }
}
