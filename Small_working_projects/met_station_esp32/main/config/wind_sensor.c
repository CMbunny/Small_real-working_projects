#include "wind_sensor.h"
#include "wind_sensor_config.h"
#include "../core/current_loop_4_20ma.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

#define WIND_ADC_OPEN_LOOP_RAW     500
#define WIND_AVG_SAMPLES           8
#define WIND_GUST_RESET_MS         10000

typedef enum {
    WIND_STATUS_OK = 0,
    WIND_STATUS_DISCONNECTED,
    WIND_STATUS_FAULT
} wind_status_t;

static adc_oneshot_unit_handle_t adc_handle;
static const char *TAG = "WIND";

static int adc_samples[WIND_AVG_SAMPLES];
static int adc_index = 0;
static bool adc_buffer_full = false;

static float wind_gust_mps = 0.0f;
static TickType_t last_gust_reset = 0;

static int adc_moving_average(int new_sample)
{
    adc_samples[adc_index++] = new_sample;

    if (adc_index >= WIND_AVG_SAMPLES) {
        adc_index = 0;
        adc_buffer_full = true;
    }

    int sum = 0;
    int count = adc_buffer_full ? WIND_AVG_SAMPLES : adc_index;

    for (int i = 0; i < count; i++) {
        sum += adc_samples[i];
    }

    return (count > 0) ? (sum / count) : new_sample;
}

static void wind_task(void *arg)
{
    last_gust_reset = xTaskGetTickCount();

    while (1) {

        int adc_raw = cl420_adc_read(adc_handle, WIND_ADC_CHANNEL);
        int adc = adc_moving_average(adc_raw);

        float voltage = 0.0f;
        float current = 0.0f;
        float wind_mps = 0.0f;
        wind_status_t status = WIND_STATUS_OK;

        if (adc < WIND_ADC_OPEN_LOOP_RAW) {

            voltage = 0.0f;
            current = 0.0f;
            wind_mps = 0.0f;
            status = WIND_STATUS_DISCONNECTED;
        }
        else {
            voltage = cl420_adc_to_voltage(adc, 12, 3.9f);

            current = cl420_voltage_to_current(
                voltage,
                WIND_SHUNT_RESISTOR_OHM
            );

            if (current < WIND_FAULT_CUTOFF_MA) {

                wind_mps = 0.0f;
                status = WIND_STATUS_FAULT;

                ESP_LOGW(TAG, "4–20mA fault: %.2f mA", current);
            }
            else if (current < WIND_ZERO_CUTOFF_MA) {

                wind_mps = 0.0f;
                status = WIND_STATUS_OK;
            }
            else {
                current = cl420_clamp(
                    current,
                    WIND_MIN_MA,
                    WIND_MAX_MA
                );

                wind_mps = cl420_current_to_units(
                    current,
                    WIND_MIN_MA,
                    WIND_MAX_MA,
                    WIND_MAX_MPS
                );
            }
        }

        if (wind_mps > wind_gust_mps) {
            wind_gust_mps = wind_mps;
        }

        if ((xTaskGetTickCount() - last_gust_reset)
            > pdMS_TO_TICKS(WIND_GUST_RESET_MS)) {

            wind_gust_mps = 0.0f;
            last_gust_reset = xTaskGetTickCount();
        }

        ESP_LOGI(TAG,
            "ADC=%d  V=%.3fV  I=%.2f mA  Wind=%.2f m/s  Gust=%.2f m/s  Status=%d",
            adc,
            voltage,
            current,
            wind_mps,
            wind_gust_mps,
            status
        );

        vTaskDelay(pdMS_TO_TICKS(WIND_TASK_PERIOD_MS));
    }
}

void wind_sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = WIND_ADC_UNIT
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            WIND_ADC_CHANNEL,
            &chan_cfg
        )
    );

    ESP_LOGI(TAG, "Wind sensor initialized");
}

void wind_sensor_start(void)
{
    xTaskCreate(
        wind_task,
        "wind_task",
        WIND_TASK_STACK_SIZE,
        NULL,
        WIND_TASK_PRIORITY,
        NULL
    );
}
