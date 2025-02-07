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
void init_opentherm();
void um_ot_update_state(bool otch, int otdhw, int ottbsp);