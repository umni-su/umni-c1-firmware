#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_adc/adc_oneshot.h"
#include "ntc_driver.h"
#include "../config/config.h"
#include "../../main/includes/events.h"
#include "../automation/automation.h"
#include "adc.h"

static const char *ADC_TAG = "adc";

static TaskHandle_t adc_task_handle = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static ntc_device_handle_t ntc_chan_1_handle = NULL;
static ntc_device_handle_t ntc_chan_2_handle = NULL;

float ntc_data[TOTAL_NTC_CHANNELS] = {0.0};

static adc_cali_handle_t adc_cali_ai1_handle = NULL;
static adc_cali_handle_t adc_cali_ai2_handle = NULL;
static int adc_raw[2][10];
static int voltage[2][10];

static bool do_calibration1_chan0 = false;
static bool do_calibration1_chan1 = false;

static short int ai_loop_count = 0;
static short int ntc_loop_count = 0;

static adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
};

static um_adc_config_t adc_config[TOTAL_ANALOG_CHANNELS];

static um_am_main_t ai_automations[TOTAL_ANALOG_CHANNELS];

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

/**
 * Method first
 * Initialize ntc channels
 */
esp_err_t initialize_ntc_channels()
{
    esp_err_t res = ESP_OK;
    ntc_config_t ntc_config = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 10000,
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

/**
 * Method second
 * Initialize adc channels
 */
esp_err_t initialize_ai_channels()
{
    esp_err_t res;
    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_6,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    res = adc_oneshot_config_channel(adc_handle, AN_INP_1, &config);
    res = adc_oneshot_config_channel(adc_handle, AN_INP_2, &config);

    do_calibration1_chan0 = um_adc_calibration_init(ADC_UNIT_1, AN_INP_1, ADC_ATTEN_DB_6, &adc_cali_ai1_handle);
    do_calibration1_chan1 = um_adc_calibration_init(ADC_UNIT_1, AN_INP_2, ADC_ATTEN_DB_6, &adc_cali_ai2_handle);

    return res;
}

void ai_queue_task()
{
    while (true)
    {
        esp_err_t err;

        err = adc_oneshot_read(adc_handle, AN_INP_1, &adc_raw[0][0]);
        if (err != ESP_OK)
            continue;
        ESP_LOGI(ADC_TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, AN_INP_1, adc_raw[0][0]);
        if (do_calibration1_chan0)
        {
            err = adc_cali_raw_to_voltage(adc_cali_ai1_handle, adc_raw[0][0], &voltage[0][0]);
            if (err != ESP_OK)
                continue;
            ESP_LOGI(ADC_TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, AN_INP_1, voltage[0][0]);
        }
        um_ev_message_ai ai1 = {
            .channel = AN_INP_1,
            .value = adc_raw[0][0],
            .voltage = voltage[0][1]};
        um_adc_update_values(AN_INP_1, ai1.value, ai1.voltage);

        vTaskDelay(pdMS_TO_TICKS(1000));

        err = adc_oneshot_read(adc_handle, AN_INP_2, &adc_raw[0][1]);
        if (err != ESP_OK)
            continue;
        float lux = (adc_raw[0][1] * 100) / 4095;
        ESP_LOGI(ADC_TAG, "ADC%d Channel[%d] Raw Data: %d, LUX: %0.1f", ADC_UNIT_1 + 1, AN_INP_2, adc_raw[0][1], lux);
        if (do_calibration1_chan1)
        {
            err = adc_cali_raw_to_voltage(adc_cali_ai2_handle, adc_raw[0][1], &voltage[0][1]);
            if (err != ESP_OK)
                continue;
            ESP_LOGI(ADC_TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, AN_INP_2, voltage[0][1]);
        }

        um_ev_message_ai ai2 = {
            .channel = AN_INP_2,
            .value = adc_raw[0][1],
            .voltage = voltage[0][0]};
        um_adc_update_values(AN_INP_2, ai2.value, ai2.voltage);

        if (ai_loop_count >= NOTIFICATION_LOOP_COUNT)
        {
            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_AI, &ai2, sizeof(um_ev_message_ai), portMAX_DELAY);

            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_AI, &ai1, sizeof(um_ev_message_ai), portMAX_DELAY);

            ai_loop_count = 0;
        }
        else
        {
            ai_loop_count++;
        }
        vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void um_ntc_process_channel_request(analog_inputs_t channel)
{
    float temp;
    short int index = channel == AN_NTC_1 ? 0 : 1;
    ntc_device_handle_t ntc_read_handle = channel == AN_NTC_1 ? ntc_chan_1_handle : ntc_chan_2_handle;
    if (ntc_dev_get_temperature(ntc_read_handle, &ntc_data[index]) == ESP_OK)
    {
        temp = ntc_data[index];
        ESP_LOGI(ADC_TAG, "NTC[%d] temperature = %.2f C", channel, temp);
        um_adc_update_values(channel, temp, 0);
    }
}

void ntc_queue_task(void *arg)
{
    // ntc_device_handle_t handle = (ntc_device_handle_t)arg;
    //  analog_inputs_t chan;
    //  int index = 0;
    //   if (handle == ntc_chan_1_handle)
    //   {
    //       index = 0;
    //       chan = AN_NTC_1;
    //   }
    //   else
    //   {
    //       index = 1;
    //       chan = AN_NTC_2;
    //   }
    while (true)
    {
        // NTC 1
        um_ntc_process_channel_request(AN_NTC_1);
        // NTC 2
        um_ntc_process_channel_request(AN_NTC_2);

        if (ntc_loop_count >= NOTIFICATION_LOOP_COUNT)
        {
            um_ev_message_ntc message;

            // Send notification NTC1
            message.channel = AN_NTC_1;
            message.temp = ntc_data[0];
            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_NTC, &message, sizeof(message), portMAX_DELAY);
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            // Send notification NTC2
            message.channel = AN_NTC_2;
            message.temp = ntc_data[1];
            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_NTC, &message, sizeof(message), portMAX_DELAY);

            vTaskDelay(30 / portTICK_PERIOD_MS);
            ntc_loop_count = 0;
        }
        else
        {
            ntc_loop_count++;
        }

        // if (ntc_dev_get_temperature(handle, &ntc_data[index]) == ESP_OK)
        // {
        //     float temp = get_ntc_data_channel_temp(chan);
        //     ESP_LOGI(ADC_TAG, "NTC CHANNEL %d temperature = %.2f C", chan, temp);
        //     um_ev_message_ntc message = {
        //         .channel = chan,
        //         .temp = temp};
        //     um_adc_update_values(chan, message.temp, 0);
        //     esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_NTC, &message, sizeof(message), portMAX_DELAY);
        // }
        vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(ntc_chan_1_handle));
    ESP_ERROR_CHECK(ntc_dev_delete(ntc_chan_2_handle));
    vTaskDelete(NULL);
}

/**
 * Main method
 * Initialize all and register tasks
 */
void init_adc()
{
    // Get config file or create empty file if not exists
    um_adc_get_config_file();
    ESP_LOGI(ADC_TAG, "Register NTC channels");
    esp_err_t res = ESP_OK;
    res = adc_oneshot_new_unit(&init_config, &adc_handle);
    res = initialize_ntc_channels();
    // and AFTER:
    res = initialize_ai_channels();

    if (res != ESP_OK)
    {
        // TODO - blink err
        ESP_LOGE(ADC_TAG, "Failed init ADC with code: %s", esp_err_to_name(res));
    }
    else
    {
        xTaskCreatePinnedToCore(ntc_queue_task, "ntc_task", 4096, NULL, 4, &adc_task_handle, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        // xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 3, &adc_task_handle, 1);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        xTaskCreatePinnedToCore(ai_queue_task, "ai_task", 4096, NULL, 4, &adc_task_handle, 1);
    }

    // vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    // xTaskCreatePinnedToCore(ntc_queue_task, "ntc2_task", 4096, (void *)ntc_chan_2_handle, 10, &adc_task_handle, 1);
}

void um_adc_get_config_file()
{
    char *contents = um_config_get_config_file(CONFIG_FILE_AI);

    cJSON *config = NULL;

    if (contents == NULL)
    {
        // AI config must be an object "{}" case all config is {"ntc1": {...},"ntc2": {...},"ai1": {...},"ai2": {...}}
        config = cJSON_CreateObject();
        if (um_config_write_config_file(CONFIG_FILE_AI, config))
        {
            contents = cJSON_PrintUnformatted(config);
            ESP_LOGI(ADC_TAG, "AI Config file created successfully: %s", contents);
        }
    }
    else
    {
        ESP_LOGI(ADC_TAG, "AI Config file is: %s", contents);
        um_adc_add_sensors_from_config();
    }
    free(contents);
    cJSON_Delete(config);
}

/**
 * Update adc channel values
 *
 * @param   int   value    new value (temp or adc value)
 * @param   int   voltage  new value of voltage or 0
 *
 * @return  bool
 */
bool um_adc_update_values(analog_inputs_t chan, float value, int voltage)
{
    for (size_t i = 0; i < TOTAL_ANALOG_CHANNELS; i++)
    {
        if (adc_config[i].channel == chan)
        {
            adc_config[i].value = value;
            adc_config[i].voltage = voltage;

            ai_automations[i].value = value;
            um_am_automation_run(&ai_automations[i]);

            return true;
        }
    }
    return false;
}

void um_adc_add_sensors_from_config()
{
    // обнуляем массив сенсоров
    for (size_t i = 0; i < TOTAL_ANALOG_CHANNELS; i++)
    {
        adc_config[i].en = false;
        adc_config[i].channel = -1;
        adc_config[i].type = 0;
        // todo обновлять индексы ntc1 ntc2 ai1 ai2
    }
    char *config = um_config_get_config_file(CONFIG_FILE_AI);

    cJSON *array = cJSON_Parse(config);
    cJSON *el = NULL;
    int index = 0;
    cJSON_ArrayForEach(el, array)
    {
        bool has_channel = cJSON_HasObjectItem(el, "channel") && cJSON_GetObjectItem(el, "channel") != NULL;
        if (has_channel)
        {
            cJSON *en = cJSON_GetObjectItem(el, "en");
            adc_config[index].en = en != NULL ? cJSON_IsTrue(en) : false;
            adc_config[index].type = cJSON_HasObjectItem(el, "type") ? cJSON_GetObjectItem(el, "type")->valueint : 0;
            adc_config[index].channel = cJSON_HasObjectItem(el, "channel") ? cJSON_GetObjectItem(el, "channel")->valueint : 0;

            if (adc_config[index].channel == AN_INP_1)
            {
                adc_config[index].value = adc_raw[0][0];
                adc_config[index].voltage = voltage[0][0];
            }
            else if (adc_config[index].channel == AN_INP_2)
            {
                adc_config[index].value = adc_raw[0][1];
                adc_config[index].voltage = voltage[0][1];
            }

            um_am_parse_json_config(el, &ai_automations[index]);
        }
        index++;
    }
    free((void *)config);
    cJSON_Delete(array);
}

um_adc_config_t *um_adc_get_config_config_item(analog_inputs_t chan)
{
    for (size_t i = 0; i < TOTAL_ANALOG_CHANNELS; i++)
    {
        if (adc_config[i].channel == chan)
        {
            return &adc_config[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
bool um_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(ADC_TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(ADC_TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(ADC_TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(ADC_TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

void um_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(ADC_TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(ADC_TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}