#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "esp_err.h"
#include <stdint.h>

// Initializes LEDC (PWM) for R/G/B channels.
// On failure, logs the error and restarts the device.
esp_err_t led_control_init(void);

// Sets RGB duty. Returns ESP_OK or an error code on runtime failure.
// Does NOT restart - caller decides how to handle runtime errors.
esp_err_t led_control_set_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif
