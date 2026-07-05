#include "uart_handler.h"
#include "led_control.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    17
#define UART_RX_PIN    16
#define UART_BAUD      9600
#define BUF_SIZE       128

#define FRAME_MARKER   0xFF
#define ESC_BYTE       0xFE
#define ESC_XOR        0x20

static const char *TAG = "uart_handler";

typedef enum { WAIT_MARKER, READ_R, READ_G, READ_B, READ_CHECKSUM } parse_state_t;

static void uart_rx_task(void *arg)
{
    uint8_t byte;
    parse_state_t state = WAIT_MARKER;
    bool escape_pending = false;
    uint8_t r = 0, g = 0, b = 0;

    while (1) {
        int len = uart_read_bytes(UART_PORT, &byte, 1, portMAX_DELAY);
        if (len <= 0) continue;

        if (state == WAIT_MARKER) {
            if (byte == FRAME_MARKER) {
                state = READ_R;
                escape_pending = false;
            }
            continue;
        }

        // Mid-frame: an unescaped marker means we lost sync somewhere.
        // Treat it as the start of a fresh frame instead of misreading it as data.
        if (!escape_pending && byte == FRAME_MARKER) {
            state = READ_R;
            escape_pending = false;
            continue;
        }

        uint8_t value;
        if (escape_pending) {
            value = byte ^ ESC_XOR;
            escape_pending = false;
        } else if (byte == ESC_BYTE) {
            escape_pending = true;
            continue; // wait for the byte this escapes
        } else {
            value = byte;
        }

        switch (state) {
            case READ_R:
                r = value;
                state = READ_G;
                break;
            case READ_G:
                g = value;
                state = READ_B;
                break;
            case READ_B:
                b = value;
                state = READ_CHECKSUM;
                break;
            case READ_CHECKSUM: {
                uint8_t expected = r ^ g ^ b;
                if (value == expected) {
                    esp_err_t err = led_control_set_rgb(r, g, b);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to set RGB (err=%d) - continuing, not restarting", err);
                    }
                } else {
                    ESP_LOGW(TAG, "Checksum mismatch (got=0x%02X expected=0x%02X) - frame dropped", value, expected);
                }
                state = WAIT_MARKER;
                break;
            }
            default:
                break;
        }
    }
}

esp_err_t uart_handler_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t err = uart_param_config(UART_PORT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed (err=%d), restarting", err);
        esp_restart();
    }

    err = uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed (err=%d), restarting", err);
        esp_restart();
    }

    err = uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed (err=%d), restarting", err);
        esp_restart();
    }

    BaseType_t ok = xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 10, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create uart_rx_task, restarting");
        esp_restart();
    }

    return ESP_OK;
}
