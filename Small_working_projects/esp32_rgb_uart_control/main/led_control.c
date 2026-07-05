#include "led_control.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"

#define RED_GPIO    4
#define GREEN_GPIO  5
#define BLUE_GPIO   18

static const char *TAG = "led_control";

static esp_err_t setup_channel(ledc_channel_t ch, int gpio)
{
    ledc_channel_config_t chan = {
        .gpio_num   = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = ch,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    return ledc_channel_config(&chan);
}

esp_err_t led_control_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed (err=%d), restarting", err);
        esp_restart();
    }

    err = setup_channel(LEDC_CHANNEL_0, RED_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Red channel config failed (err=%d), restarting", err);
        esp_restart();
    }

    err = setup_channel(LEDC_CHANNEL_1, GREEN_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Green channel config failed (err=%d), restarting", err);
        esp_restart();
    }

    err = setup_channel(LEDC_CHANNEL_2, BLUE_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Blue channel config failed (err=%d), restarting", err);
        esp_restart();
    }

    return ESP_OK;
}

esp_err_t led_control_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    esp_err_t err;

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, r);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_duty R failed (err=%d)", err); return err; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) { ESP_LOGE(TAG, "update_duty R failed (err=%d)", err); return err; }

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, g);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_duty G failed (err=%d)", err); return err; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    if (err != ESP_OK) { ESP_LOGE(TAG, "update_duty G failed (err=%d)", err); return err; }

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, b);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_duty B failed (err=%d)", err); return err; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
    if (err != ESP_OK) { ESP_LOGE(TAG, "update_duty B failed (err=%d)", err); return err; }

    return ESP_OK;
}
