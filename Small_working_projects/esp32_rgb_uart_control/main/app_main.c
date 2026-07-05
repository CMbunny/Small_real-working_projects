#include "led_control.h"
#include "uart_handler.h"

void app_main(void)
{
    led_control_init();   // restarts device internally on failure
    uart_handler_init();  // restarts device internally on failure, starts RX task
}
