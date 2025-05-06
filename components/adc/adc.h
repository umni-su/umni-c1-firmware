#pragma once
#include "hal/adc_types.h"
#include "ntc_driver.h"
// #include "esp_adc/adc_cali.h"
// #include "esp_adc/adc_cali_scheme.h"
// #include "cJSON.h"

#define NTC_ADC_UNIT ADC_UNIT_1
#define NTC_ADC_ATTEN ADC_ATTEN_DB_6
#define ADC_TASK_PRIORITY 13
#define ADC_TASK_TIMEOUT 10000

#define TOTAL_ANALOG_CHANNELS 4
#define TOTAL_NTC_CHANNELS 2

#define NOTIFICATION_LOOP_COUNT 10

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

/**
 * @enum um_ai_cnaggel_type_t
 * Describes available analog sensor types
 */
typedef enum
{
    AN_LIGHT_SENSOR = 1,
    AN_SOIL_MOISTURE_SENSOR = 2,
    AN_AMPERAGE_5_SENSOR = 3,
    AN_AMPERAGE_20_SENSOR = 4,
    AN_AMPERAGE_30_SENSOR = 5,
    AN_NTC_SENSOR = 6,
    AN_NTC_OTHER = 20,
} um_ai_cnannel_type_t;

/**
 * @enum analog_inputs_t
 * Describes all available analog channels
 */
typedef enum
{
    AN_NTC_1 = CONFIG_UMNI_NTC_1, // GPIO36
    AN_NTC_2 = CONFIG_UMNI_NTC_2, // GPIO39
    AN_INP_1 = CONFIG_UMNI_ADC_1, // GPIO34
    AN_INP_2 = CONFIG_UMNI_ADC_2  // GPIO35
} analog_inputs_t;

typedef struct
{
    float celsius;
    adc_channel_t channel;
    int value;
} ntc_data_t;

typedef struct
{
    um_ai_cnannel_type_t type;
    analog_inputs_t channel;
    bool en;
    float value;
    int voltage;
} um_adc_config_t;

void init_adc();

esp_err_t initialize_ntc_channels();

void um_adc_get_config_file();

void ntc_queue_task(void *ntc_data);

float get_ntc_data_channel_temp(adc_channel_t channel);

bool um_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

void um_adc_calibration_deinit(adc_cali_handle_t handle);

void um_adc_add_sensors_from_config();

bool um_adc_update_values(analog_inputs_t chan, float value, int voltage);

um_adc_config_t *um_adc_get_config_config_item(analog_inputs_t chan);
