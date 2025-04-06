#include <stdint.h>

#define MAX_SENSORS 32
#define MAX_SEARCH_SENSORS 5
#define SEARCH_TIMEOUT 15000

typedef struct
{
    uint32_t serial;
    long time;
    bool alarm;
    bool triggered;
    uint8_t state;
} um_rf_devices_t;

void um_rf_433_init();

void um_rf433_receiver_task(void *pvParameter);

short int um_rf433_get_existing_index(um_rf_devices_t *devices, uint32_t number, int max);

short int um_rf433_get_array_length(um_rf_devices_t *devices, int max);

void um_rf433_get_config_file();

void um_rf433_activale_search();

void um_rf433_clear_search();

um_rf_devices_t *um_rf433_get_search_result();

void um_rf433_add_sensors_from_config();