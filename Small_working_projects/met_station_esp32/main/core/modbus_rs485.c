#include "modbus_rs485.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "driver/uart.h"
static const char *TAG = "MODBUS";

static uart_port_t mb_uart;
//static int mb_rts_gpio;

static uint16_t modbus_crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

esp_err_t modbus_rs485_init(const modbus_rs485_cfg_t *cfg)
{
    mb_uart = cfg->uart_port;
   // mb_rts_gpio = cfg->rts_gpio;

    uart_config_t uart_cfg = {
        .baud_rate = cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(mb_uart, 256, 256, 0, NULL, 0);
    uart_param_config(mb_uart, &uart_cfg);
    uart_set_pin(mb_uart, cfg->tx_gpio, cfg->rx_gpio,
                 cfg->rts_gpio, UART_PIN_NO_CHANGE);

    uart_set_mode(mb_uart, UART_MODE_RS485_HALF_DUPLEX);

    ESP_LOGI(TAG, "RS485 Modbus initialized");
    return ESP_OK;
}

esp_err_t modbus_read_holding_registers(
    uint8_t slave_id,
    uint16_t start_reg,
    uint16_t num_regs,
    uint16_t *out_regs
)
{
    uint8_t tx[8];
    uint8_t rx[256];

    tx[0] = slave_id;
    tx[1] = 0x03;
    tx[2] = start_reg >> 8;
    tx[3] = start_reg & 0xFF;
    tx[4] = num_regs >> 8;
    tx[5] = num_regs & 0xFF;

    uint16_t crc = modbus_crc16(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = crc >> 8;

    uart_flush(mb_uart);
    uart_write_bytes(mb_uart, tx, 8);
    uart_wait_tx_done(mb_uart, pdMS_TO_TICKS(100));

    int rx_len = uart_read_bytes(
        mb_uart,
        rx,
        sizeof(rx),
        pdMS_TO_TICKS(500)
    );

    if (rx_len < 5) {
        ESP_LOGE(TAG, "No response");
        return ESP_FAIL;
    }

    uint16_t rx_crc = rx[rx_len - 2] | (rx[rx_len - 1] << 8);
    if (modbus_crc16(rx, rx_len - 2) != rx_crc) {
        ESP_LOGE(TAG, "CRC error");
        return ESP_FAIL;
    }

    if (rx[1] & 0x80) {
        ESP_LOGE(TAG, "Modbus exception %d", rx[2]);
        return ESP_FAIL;
    }

    uint8_t byte_count = rx[2];
    if (byte_count != num_regs * 2) {
        ESP_LOGE(TAG, "Invalid byte count");
        return ESP_FAIL;
    }

    for (int i = 0; i < num_regs; i++) {
        out_regs[i] = (rx[3 + i*2] << 8) | rx[4 + i*2];
    }

    return ESP_OK;
}
