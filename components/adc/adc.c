#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
// #include "adc_table.c"

#include "ntc_driver.h"

#include "adc.h"

const char *ADC_TAG = "adc";
const char *ADC_TASK_TAG = "adc-task";

adc_oneshot_unit_handle_t adc_handle;
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
};

// B57164-K 222-J, 2.2 кОм, 5%, NTC термистор

TaskHandle_t ntc_task_handle = NULL;

static adc_port_config_t adc_config[ADC_PORTS_COUNT];
// static adc_port_config_t ntc_adc_config[ADC_PORTS_COUNT];
static ntc_thermistor_config_t ntc_thermistor_config[ADC_PORTS_COUNT];

static int adc_raw[ADC_PORTS_COUNT][4];
// static int voltage[ADC_PORTS_COUNT][4];
// static void adc_calibration_deinit(adc_cali_handle_t handle);

ntc_data_t my_ntc_data[ADC_PORTS_COUNT] = {0};

// void get_adc_config(char *adc_config_name, adc_port_config_t *adc_port_config)
// {
//     int index = -1;
//     char *config_contents = get_config_file(adc_config_name);
//     if (strcmp(adc_config_name, "adc1") == 0)
//     {
//         index = 0;
//     }
//     else if (strcmp(adc_config_name, "adc2") == 0)
//     {
//         index = 1;
//     }
//     else if (strcmp(adc_config_name, "adc3") == 0)
//     {
//         index = 2;
//     }
//     else if (strcmp(adc_config_name, "adc4") == 0)
//     {
//         index = 3;
//     }

//     if (config_contents != NULL)
//     {
//         cJSON *config_json = cJSON_Parse(config_contents);
//         char *label = cJSON_GetStringValue(cJSON_GetObjectItem(config_json, "label"));
//         int channel = cJSON_GetObjectItem(config_json, "channel")->valueint;
//         int enabled = cJSON_GetObjectItem(config_json, "enabled")->valueint;

//         adc_port_config->channel = channel;
//         adc_port_config->enabled = enabled;
//         adc_port_config->label = label; // если подставить просто слово, то ок. что не так с переменной?
//         adc_port_config->type = cJSON_HasObjectItem(config_json, "type") ? cJSON_GetObjectItem(config_json, "type")->valueint : TYPE_NONE;
//         if (cJSON_HasObjectItem(config_json, "config"))
//         {
//             cJSON *_config_ = cJSON_GetObjectItem(config_json, "config");
//             switch (adc_port_config->type)
//             {
//             case TYPE_NTC_THERMISTOR:
//                 int beta = cJSON_HasObjectItem(_config_, "coef") ? cJSON_GetObjectItem(_config_, "coef")->valueint : TYPE_NONE;
//                 int width_bit = cJSON_HasObjectItem(_config_, "width_bit") ? cJSON_GetObjectItem(_config_, "width_bit")->valueint : TYPE_NONE;
//                 ntc_thermistor_config[index].beta = beta;
//                 ntc_thermistor_config[index].width_bit = width_bit;
//                 ntc_thermistor_config[index].channel = channel;
//                 break;

//             default:
//                 break;
//             }

//             free(_config_);
//         }
//         //  dont free labels ok
//     }
//     free(config_contents);
// }

void initialize_adc_channel()
{
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = NTC_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC1_CHAN1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC1_CHAN2, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC1_CHAN3, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC1_CHAN4, &config));
}

/**
 * Initialize adc thermistor
 *
 * @return  ntc_data_t Returns struct of useful values
 */
void get_ntc_termistor_data(ntc_thermistor_config_t ntc_thermistor_config, ntc_data_t *ntc_data)
{

    if (
        ntc_thermistor_config.channel == ADC_CHANNEL_0 ||
        ntc_thermistor_config.channel == ADC_CHANNEL_3 ||
        ntc_thermistor_config.channel == ADC_CHANNEL_6 ||
        ntc_thermistor_config.channel == ADC_CHANNEL_7)
    {
        int channel = ntc_thermistor_config.channel;
        int beta = ntc_thermistor_config.beta;
        int width_bit = ntc_thermistor_config.width_bit;

        int index = 0;
        switch (channel)
        {
        case ADC1_CHAN1:
            index = 0;
            break;
        case ADC1_CHAN2:
            index = 1;
            break;
        case ADC1_CHAN3:
            index = 2;
            break;
        case ADC1_CHAN4:
            index = 3;
            break;

        default:
            break;
        }

        // adc_cali_handle_t adc_cali_chan_handle = NULL;
        //  !!! TODO - калибровка заполняет массив, и съедается немного памяти (adc_raw[4][10],voltage[4][10]) нужно его очищать ну или посмотреть сколько занимает буфер
        /* bool do_calibration_chan = adc_calibration_init(ADC_UNIT_1, channel, NTC_ADC_ATTEN, &adc_cali_chan_handle);

         if (do_calibration_chan)
         {
             ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_chan_handle, adc_raw[index][0], &voltage[index][0]));
             // ESP_LOGI(ADC_TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, channel, voltage[index][0]);
         }*/

        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &adc_raw[index][0]));

        int width_dec_max = (1 << width_bit) - 1; // 2 ^ WIDTH_BIT - 1.
        float celsius = 1 / (log(1 / ((float)width_dec_max / adc_raw[index][0] - 1)) / beta + 1.0 / 298.15) - 273.15;

        ntc_data->celsius = celsius;
        ntc_data->value = adc_raw[index][0];
        ntc_data->channel = channel;

        // ESP_LOGI(ADC_TAG, "NTC data from adc.c. celsius: %f, value: %d, channel %d ", celsius, adc_raw[index][0], channel);
    }
    else
    {
        ESP_LOGE(ADC_TAG, "Error! Wrong channel");
    }
}
void ntc_queue_task(void *arg)
{
    ntc_config_t ntc_config = {
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 11000,
        .vdd_mv = 3300,
        .circuit_mode = CIRCUIT_MODE_NTC_GND,
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL_7,
        .unit = ADC_UNIT_1};

    // Create the NTC Driver and Init ADC
    ntc_device_handle_t ntc = NULL;
    adc_oneshot_unit_handle_t adc_handle = NULL;

    ESP_ERROR_CHECK(ntc_dev_create(&ntc_config, &ntc, &adc_handle));
    ESP_ERROR_CHECK(ntc_dev_get_adc_handle(ntc, &adc_handle));
    while (true)
    {

        float temp = 0.0;

        if (ntc_dev_get_temperature(ntc, &temp) == ESP_OK)
        {
            ESP_LOGI(ADC_TAG, "NTC temperature = %.2f C", temp);
        }

        vTaskDelay(ADC_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(ntc_dev_delete(ntc));
    vTaskDelete(NULL);
}

void init_adc()
{
    ESP_LOGI(ADC_TAG, "ADC Start");
    // initialize_adc_channel();

    // get_adc_config("adc1", &adc_config[0]);
    //  ESP_LOGI(ADC_TAG, "[adc1] %s, %d, free heap : %ld bytes", adc_config[0].label, adc_config[0].channel, esp_get_free_heap_size());

    // get_adc_config("adc2", &adc_config[1]);
    //  ESP_LOGI(ADC_TAG, "[adc2] %s, %d, free heap : %ld bytes", adc_config[1].label, adc_config[1].channel, esp_get_free_heap_size());

    /// get_adc_config("adc3", &adc_config[2]);
    // ESP_LOGI(ADC_TAG, "[adc3] %s, %d, free heap : %ld bytes", adc_config[2].label, adc_config[2].channel, esp_get_free_heap_size());

    // get_adc_config("adc4", &adc_config[3]);
    //  ESP_LOGI(ADC_TAG, "[adc4] %s, %d, free heap : %ld bytes", adc_config[3].label, adc_config[3].channel, esp_get_free_heap_size());

    xTaskCreatePinnedToCore(ntc_queue_task, "ntc_task", 4096, NULL, ONE_WIRE_TASK_PRIORITY, &ntc_task_handle, 1);
    //  configASSERT(ntc_task_handle);
}
