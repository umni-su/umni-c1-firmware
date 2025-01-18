#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

#include "ntc_driver.h"

#include "adc.h"

const char *ADC_TAG = "adc";
const char *ADC_TASK_TAG = "adc-task";

static adc_port_config_t adc_config[TOTAL_ANALOG_CHANNELS];
static ntc_thermistor_config_t ntc_thermistor_config[TOTAL_NTC_CHANNELS];

TaskHandle_t ntc_task_handle = NULL;
adc_oneshot_unit_handle_t adc_handle = NULL;
ntc_data_t my_ntc_data[TOTAL_NTC_CHANNELS] = {0};
ntc_device_handle_t ntc_chan_1_handle = NULL;
ntc_device_handle_t ntc_chan_2_handle = NULL;

ntc_device_handle_t *get_ntc_handler(analog_inputs_t channel)
{
    if (channel == AN_NTC_1)
    {
        return &ntc_chan_1_handle;
    }
    return &ntc_chan_2_handle;
}

void initialize_ntc_channel(analog_inputs_t ntc_channel, ntc_device_handle_t *handle, int beta_value)
{
    // BETA = 3950
    ntc_config_t ntc_config = {
        .b_value = beta_value,
        .r25_ohm = 10000,
        .fixed_ohm = 11000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_12,
        .channel = ntc_channel,
        .unit = ADC_UNIT_1};
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config, handle, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(handle, &adc_handle));
}

void ntc_queue_task(void *arg)
{
    ntc_device_handle_t handle = (ntc_device_handle_t)arg;
    analog_inputs_t chan;
    if (handle == ntc_chan_1_handle)
    {
        chan = AN_NTC_1;
    }
    else
    {
        chan = AN_NTC_2;
    }
    while (true)
    {
        float temp = 0.0;

        if (ntc_dev_get_temperature(handle, &temp) == ESP_OK)
        {
            ESP_LOGI(ADC_TAG, "NTC CHANNEL %d temperature = %.2f C", chan, temp);
        }
        vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(handle));
    vTaskDelete(NULL);
}

void init_adc()
{
    ESP_LOGI(ADC_TAG, "Register NTC channels");
    ntc_config_t ntc_config1 = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 11000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_12,
        .channel = AN_NTC_1,
        .unit = ADC_UNIT_1};
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config1, &ntc_chan_1_handle, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc_chan_1_handle, &adc_handle));

    ntc_config_t ntc_config2 = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 11000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_12,
        .channel = AN_NTC_2,
        .unit = ADC_UNIT_1};
    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config2, &ntc_chan_2_handle, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc_chan_2_handle, &adc_handle));
    // initialize_ntc_channel(AN_NTC_1, &ntc_chan_1_handle, 3950);
    //  initialize_ntc_channel(AN_NTC_2, &ntc_chan_2_handle, 3950);

    xTaskCreatePinnedToCore(ntc_queue_task, "ntc1_task", 4096, (void *)ntc_chan_1_handle, 10, &ntc_task_handle, 1);
    xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 10, &ntc_task_handle, 1);

    // vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    // xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 10, &ntc_task_handle, 1);
}
