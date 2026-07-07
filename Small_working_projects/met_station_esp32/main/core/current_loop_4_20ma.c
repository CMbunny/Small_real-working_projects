#include "current_loop_4_20ma.h"
#include "esp_adc/adc_oneshot.h"


int cl420_adc_read(void *adc_handle, int channel)
{
    int adc = 0;
    esp_err_t err = adc_oneshot_read(adc_handle, channel, &adc);
    if (err != ESP_OK) {
        return -1;   // sentinel: read failed, distinct from a real 0 reading
    }
    return adc;
}

float cl420_adc_to_voltage(int adc, int bits, float vref)
{
    float max = (1 << bits) - 1;
    return ((float)adc / max) * vref;
}

float cl420_voltage_to_current(float voltage, float shunt_ohm)
{
    return (voltage / shunt_ohm) * 1000.0f;
}

float cl420_current_to_units(float current,
                             float min_ma,
                             float max_ma,
                             float max_units)
{
    float span = max_ma - min_ma;
    return (current - min_ma) * (max_units / span);
}

int cl420_is_fault(float current, float min_ma, float max_ma)
{
    if (current < (min_ma - 0.4f)) return 1;
    if (current > (max_ma + 1.0f)) return 1;
    return 0;
}

float cl420_clamp(float val, float min, float max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
