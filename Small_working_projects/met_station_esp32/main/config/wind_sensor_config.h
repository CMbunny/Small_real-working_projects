#pragma once

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"  


/* ================= HARDWARE CONFIG ================= */
#define WIND_ADC_GPIO           GPIO_NUM_1
#define WIND_ADC_UNIT           ADC_UNIT_1
#define WIND_ADC_CHANNEL        ADC_CHANNEL_0  // GPIO1 on ESP32-S3

/* ================= ELECTRICAL CONFIG ================= */
#define WIND_SHUNT_RESISTOR_OHM  100.0f

/* ================= SENSOR RANGE ================= */
#define WIND_MIN_MA             4.0f
#define WIND_MAX_MA             20.0f
#define WIND_MAX_MPS            30.0f
/* ================= ZERO STABILITY CONFIG ================= */
#define WIND_ZERO_CUTOFF_MA     3.8f   // anything below → FORCE ZERO
#define WIND_FAULT_CUTOFF_MA    3.0f   // below this → FAULT

/* ================= TASK CONFIG ================= */
#define WIND_TASK_STACK_SIZE    4096
#define WIND_TASK_PRIORITY      5
#define WIND_TASK_PERIOD_MS     1000
