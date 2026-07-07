#pragma once

#include "stdint.h"
#include "esp_err.h"
#include "driver/uart.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t slave_id;
    uint32_t baudrate;
    uint8_t uart_port;
    int tx_gpio;
    int rx_gpio;
    int rts_gpio;
} modbus_rs485_cfg_t;

esp_err_t modbus_rs485_init(const modbus_rs485_cfg_t *cfg);

esp_err_t modbus_read_holding_registers(
    uint8_t slave_id,
    uint16_t start_reg,
    uint16_t num_regs,
    uint16_t *out_buf
);

#ifdef __cplusplus
}
#endif
