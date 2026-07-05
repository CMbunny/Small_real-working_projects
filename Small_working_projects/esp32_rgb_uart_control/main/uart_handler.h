#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "esp_err.h"

// Initializes UART, starts the RX parser task.
// On failure, logs the error and restarts the device.
esp_err_t uart_handler_init(void);

#endif
