#include "esp_log.h"
#include "config/sensor_select.h"

#if USE_WIND_SENSOR
#include "config/wind_sensor.h"
#endif

#if USE_PYRANOMETER
#include "config/pyranometer.h"
#endif

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "System started");

#if USE_WIND_SENSOR
    wind_sensor_init();
    wind_sensor_start();
#endif

#if USE_PYRANOMETER
    pyranometer_init();
    pyranometer_start();
#endif
}
