#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* ================= INTERFACE SELECTION ================= */
/* Choose ONLY ONE */
#define PYR_INTERFACE_4_20MA
// #define PYR_INTERFACE_RS485

/* ================= 4–20 mA CONFIG ================= */
#ifdef PYR_INTERFACE_4_20MA

#define PYR_ADC_UNIT           ADC_UNIT_1
#define PYR_ADC_CHANNEL        ADC_CHANNEL_0
#define PYR_ADC_GPIO           GPIO_NUM_1

#define PYR_SHUNT_RESISTOR_OHM  120.0f
#define PYR_MIN_MA             4.0f
#define PYR_MAX_MA             20.0f
#define PYR_MAX_WM2            2000.0f  

#endif

/* ================= RS485 CONFIG ================= */
#ifdef PYR_INTERFACE_RS485

#define PYR_MB_SLAVE_ID        1
#define PYR_MB_START_REG       0x0000
#define PYR_MB_NUM_REGS        1   // example: 32-bit value
#define PYR_MB_SCALE           0.1f

#endif

/* ================= TASK ================= */
#define PYR_TASK_STACK_SIZE    4096
#define PYR_TASK_PRIORITY      5
#define PYR_TASK_PERIOD_MS     1000

#if defined(PYR_INTERFACE_4_20MA) && defined(PYR_INTERFACE_RS485)
#error "Only ONE pyranometer interface may be enabled"
#endif

#if !defined(PYR_INTERFACE_4_20MA) && !defined(PYR_INTERFACE_RS485)
#error "No pyranometer interface selected"
#endif
