#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_event.h"

#include "ntc_driver.h"

#include "adc.h"
#include "../../main/includes/events.h"

const char *ADC_TAG = "adc";
const char *ADC_TASK_TAG = "adc-task";

// static adc_port_config_t adc_config[TOTAL_ANALOG_CHANNELS];
// static ntc_thermistor_config_t ntc_thermistor_config[TOTAL_NTC_CHANNELS];

TaskHandle_t ntc_task_handle = NULL;
adc_oneshot_unit_handle_t adc_handle = NULL;
ntc_data_t my_ntc_data[TOTAL_NTC_CHANNELS] = {0};
ntc_device_handle_t ntc_chan_1_handle = NULL;
ntc_device_handle_t ntc_chan_2_handle = NULL;
ntc_device_handle_t handle = NULL;

float ntc_data[TOTAL_NTC_CHANNELS] = {0.0};

adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
};

float get_ntc_data_channel_temp(adc_channel_t channel)
{
    switch (channel)
    {
    case AN_NTC_1:
        return ntc_data[0];
    case AN_NTC_2:
        return ntc_data[1];
    default:
        return 0.0;
        break;
    }
}

esp_err_t initialize_ntc_channels()
{
    esp_err_t res = ESP_OK;
    ntc_config_t ntc_config = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 11000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_12,
        .channel = AN_NTC_1,
        .unit = ADC_UNIT_1};
    res = ntc_dev_create(&ntc_config, &ntc_chan_1_handle, &adc_handle);
    res = ntc_dev_get_adc_handle(ntc_chan_1_handle, &adc_handle);

    ntc_config_t ntc_config_second = ntc_config;
    ntc_config_second.channel = AN_NTC_2;
    res = ntc_dev_create(&ntc_config_second, &ntc_chan_2_handle, &adc_handle);
    res = ntc_dev_get_adc_handle(ntc_chan_2_handle, &adc_handle);
    return res;
}

void ntc_queue_task(void *arg)
{
    esp_err_t res = ESP_OK;
    ntc_device_handle_t handle = (ntc_device_handle_t)arg;
    analog_inputs_t chan;
    int index = 0;
    if (handle == ntc_chan_1_handle)
    {
        index = 0;
        chan = AN_NTC_1;
    }
    else
    {
        index = 1;
        chan = AN_NTC_2;
    }
    while (true)
    {
        if (ntc_dev_get_temperature(handle, &ntc_data[index]) == ESP_OK)
        {
            float temp = get_ntc_data_channel_temp(chan);
            ESP_LOGI(ADC_TAG, "NTC CHANNEL %d temperature = %.2f C", chan, temp);
            um_ev_message_ntc message = {
                .channel = chan,
                .temp = temp};
            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_NTC, &message, sizeof(message), portMAX_DELAY);
        }
        else
        {
            res = ESP_FAIL;
        }
        vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(handle));
    vTaskDelete(NULL);
}

void init_adc()
{
    ESP_LOGI(ADC_TAG, "Register NTC channels");
    esp_err_t res = ESP_OK;
    res = adc_oneshot_new_unit(&init_config, &adc_handle);
    res = initialize_ntc_channels();

    if (res != ESP_OK)
    {
        // TODO - blink err
        ESP_LOGE(ADC_TAG, "Failed init ADC with code: %s", esp_err_to_name(res));
    }
    else
    {
        xTaskCreatePinnedToCore(ntc_queue_task, "ntc1_task", 4096, (void *)ntc_chan_1_handle, 10, &ntc_task_handle, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 10, &ntc_task_handle, 1);
    }

    // vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    // xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 10, &ntc_task_handle, 1);
}
