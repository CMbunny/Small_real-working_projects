#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_adc/adc_oneshot.h"

/* ================= API ================= */

/* ADC helpers */

float cl420_adc_to_voltage(int raw, int adc_bits, float vref);

/* Current loop math */
float cl420_voltage_to_current(float voltage, float shunt_ohms);
float cl420_clamp(float val, float min, float max);
float cl420_current_to_units(float mA, float min_ma, float max_ma, float max_units);

/* Fault detection */
int cl420_is_fault(float mA, float min_ma, float max_ma);
int cl420_adc_read(void *adc_handle, int channel);

#ifdef __cplusplus
}
#endif
