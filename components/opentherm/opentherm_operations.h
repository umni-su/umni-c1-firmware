typedef struct
{
    int status;
    bool central_heating_active;
    bool hot_water_active;
    bool flame_on;
    float modulation;
    bool is_fault;
    char fault_code;
    bool otch;   // from NVS
    int otdhwsp; // from NVS
    int ottbsp;  // from NVS
    float return_temperature;
    float dhw_temperature;
    float boiler_temperature;
    float pressure;
    unsigned long slave_product_version;
    float slave_ot_version;

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