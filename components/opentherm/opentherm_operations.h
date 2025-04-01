#include "opentherm_struct.h"

typedef struct
{
    bool adapter_success;
    bool otch;   // from NVS
    int otdhwsp; // from NVS
    int ottbsp;  // from NVS
    bool ch2;    // from NVS
    bool ototc;  // from NVS
    bool hwa;    // from NVS
    int status;
    bool central_heating_active;
    bool hot_water_active;
    bool flame_on;
    float modulation;
    bool is_fault;
    char fault_code;
    float return_temperature;
    float dhw_temperature;
    float boiler_temperature;
    float pressure;
    unsigned long slave_product_version;
    float slave_ot_version;
    float ch_max_setpoint;
    float dhw_setpoint;
    float outside_temperature;
    float flow_rate;

    float flow_rate_ch2;
    float mod; // modulation level setting
    esp_ot_min_max_t dhw_min_max;
    esp_ot_min_max_t ch_min_max;
    esp_ot_cap_mod_t cap_mod;
    esp_ot_asf_flags_t asf_flags;
    esp_ot_slave_config_t slave_config;

} um_ot_data_t;

um_ot_data_t um_ot_get_data();

void esp_ot_control_task_handler(void *pvParameter);

void um_ot_init();

esp_err_t um_ot_set_boiler_status(
    bool enable_central_heating,
    bool enable_hot_water,
    bool enable_cooling,
    bool enable_outside_temperature_compensation,
    bool enable_central_heating2);

void um_ot_set_boiler_temp(float temp);

void um_ot_set_dhw_setpoint(float temp);

void um_ot_update_state(bool otch, int otdhw, int ottbsp);

void um_ot_set_central_heating_active(bool state);

void um_ot_set_hot_water_active(bool state);

void um_ot_reset_error();

void um_ot_set_ch2(bool active);

void um_ot_set_outside_temp_comp(bool state);

void um_ot_set_modulation_level(char level);

void um_ot_set_hot_water_active(bool hwa);