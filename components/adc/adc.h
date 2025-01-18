#pragma once

#include "ntc_driver.h"
// #include "esp_adc/adc_cali.h"
// #include "esp_adc/adc_cali_scheme.h"
// #include "cJSON.h"

#define NTC_ADC_UNIT ADC_UNIT_1
#define NTC_ADC_ATTEN ADC_ATTEN_DB_6
#define ONE_WIRE_TASK_PRIORITY 13
#define ADC_TASK_TIMEOUT 1000

#define TOTAL_ANALOG_CHANNELS 4
#define TOTAL_NTC_CHANNELS 2
#define ADC_MAX_LABEL_LENGTH 16

#define NTC_READ_LEN 256

/**
 *  ADC1_CHANNEL_0 = 0, !< ADC1 channel 0 is GPIO36
    ADC1_CHANNEL_1,     !< ADC1 channel 1 is GPIO37
    ADC1_CHANNEL_2,     !< ADC1 channel 2 is GPIO38
    ADC1_CHANNEL_3,     !< ADC1 channel 3 is GPIO39
    ADC1_CHANNEL_4,     !< ADC1 channel 4 is GPIO32
    ADC1_CHANNEL_5,     !< ADC1 channel 5 is GPIO33
    ADC1_CHANNEL_6,     !< ADC1 channel 6 is GPIO34
    ADC1_CHANNEL_7,     !< ADC1 channel 7 is GPIO35
*/

typedef enum
{
    AN_NTC_1 = 0, // GPIO36
    AN_NTC_2 = 3, // GPIO39
    AN_INP_1 = 6, // GPIO34
    AN_INP_2 = 2  // GPIO35
} analog_inputs_t;

/// @brief Тут хранится элемент конфигурации канала: активен ли он и его метка
typedef struct
{
    analog_inputs_t channel;
    bool enabled;
    int type;
    char *label;
} adc_port_config_t;

typedef struct
{
    int channel;
    int beta;
} ntc_thermistor_config_t;

typedef struct
{
    float celsius;
    adc_channel_t channel;
    int value;
} ntc_data_t;

void init_adc();

ntc_device_handle_t get_ntc_handle(analog_inputs_t chan);

void initialize_ntc_channel(analog_inputs_t ntc_channel, ntc_device_handle_t *handle, int beta_value);

void get_adc_config(char *adc_config_name, adc_port_config_t *adc_port_config);

void get_ntc_termistor_data(ntc_thermistor_config_t ntc_thermistor_config, ntc_data_t *ntc_data);

adc_port_config_t get_ntc_adc_port_config(adc_channel_t adc_channel);

void ntc_queue_task(void *ntc_data);
