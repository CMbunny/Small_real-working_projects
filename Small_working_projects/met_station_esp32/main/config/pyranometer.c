#include "pyranometer.h"
#include "pyranometer_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifdef PYR_INTERFACE_4_20MA
#include "../core/current_loop_4_20ma.h"
#include "esp_adc/adc_oneshot.h"
#endif

#ifdef PYR_INTERFACE_RS485
#include "../core/modbus_rs485.h"
#endif

#ifdef PYR_INTERFACE_4_20MA
static adc_oneshot_unit_handle_t adc_handle;
#endif

static const char *TAG = "PYR";

#define ADC_OPEN_LOOP_RAW 500

static void pyranometer_task(void *arg)
{
    ESP_LOGI(TAG, "Pyranometer task running");

    while (1) {

#ifdef PYR_INTERFACE_4_20MA

        int adc = cl420_adc_read(adc_handle, PYR_ADC_CHANNEL);

        float voltage   = 0.0f;
        float current   = 0.0f;
        float radiation = 0.0f;

        if (adc < ADC_OPEN_LOOP_RAW) {

            radiation = 0.0f;
            current   = 0.0f;
        }
        else {
            voltage = cl420_adc_to_voltage(adc, 12, 3.3f);

            current = cl420_voltage_to_current(
                voltage,
                PYR_SHUNT_RESISTOR_OHM
            );

            if (cl420_is_fault(current, PYR_MIN_MA, PYR_MAX_MA)) {

                radiation = 0.0f;

                ESP_LOGE(TAG, "Pyranometer fault! I=%.2f mA", current);
            }
            else {
                current = cl420_clamp(current, PYR_MIN_MA, PYR_MAX_MA);

                radiation = cl420_current_to_units(
                    current,
                    PYR_MIN_MA,
                    PYR_MAX_MA,
                    PYR_MAX_WM2
                );
            }
        }

        radiation = cl420_clamp(radiation, 0.0f, PYR_MAX_WM2);

        ESP_LOGI(TAG,
                 "[4–20mA] ADC=%d  V=%.3fV  I=%.2f mA  Rad=%.1f W/m²",
                 adc, voltage, current, radiation);

#endif

#ifdef PYR_INTERFACE_RS485

        uint16_t regs[PYR_MB_NUM_REGS];

        if (modbus_read_holding_registers(
                PYR_MB_SLAVE_ID,
                PYR_MB_START_REG,
                PYR_MB_NUM_REGS,
                regs) == ESP_OK) {

            uint32_t raw = regs[0];
            float radiation = raw * PYR_MB_SCALE;

            ESP_LOGI(TAG, "[RS485] Radiation=%.1f W/m²", radiation);
        }
        else {
            ESP_LOGE(TAG, "RS485 read failed");
        }

#endif

        vTaskDelay(pdMS_TO_TICKS(PYR_TASK_PERIOD_MS));
    }
}

void pyranometer_init(void)
{
#ifdef PYR_INTERFACE_4_20MA

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = PYR_ADC_UNIT
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            PYR_ADC_CHANNEL,
            &chan_cfg
        )
    );

    ESP_LOGI(TAG, "Pyranometer 4–20 mA initialized");

#endif

#ifdef PYR_INTERFACE_RS485

    modbus_rs485_cfg_t cfg = {
        .slave_id  = 1,
        .baudrate  = 9600,
        .uart_port = UART_NUM_1,
        .tx_gpio   = GPIO_NUM_17,
        .rx_gpio   = GPIO_NUM_18,
        .rts_gpio  = GPIO_NUM_16
    };

    ESP_ERROR_CHECK(modbus_rs485_init(&cfg));
    ESP_LOGI(TAG, "Pyranometer RS485 initialized");

#endif
}

void pyranometer_start(void)
{
    xTaskCreate(
        pyranometer_task,
        "pyranometer_task",
        PYR_TASK_STACK_SIZE,
        NULL,
        PYR_TASK_PRIORITY,
        NULL
    );
}
