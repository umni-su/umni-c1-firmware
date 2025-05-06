#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "onewire.h"
#include "ds18x20.h"

#define ONEWIRE_TASK_TIMEOUT CONFIG_UMNI_TASK_TIMEOUT_BASE
#define ONE_WIRE_PIN CONFIG_UMNI_ONEWIRE_PIN // hc32esp default 1wire pin
#define ONE_WIRE "onewire"
#define ONEWIRE_MAX_SENSORS 10 // max sensors on bus
#define ONE_WIRE_TASK_PRIORITY 3

typedef struct
{
    bool active;
    uint64_t address;
    float value;
} um_onewire_sensor_t;

void um_onewire_prepare_config_file();

void onewire_configure();

void onewire_task(void *arg);

void um_onewire_init();

void onewire_addr_str_to_uint64_t(char *address, uint64_t *out);

void onewire_uint64_t_to_addr_str(onewire_addr_t address, char *out);

um_onewire_sensor_t *onewire_get_sensors();

esp_err_t ds18x20_read_temp(onewire_addr_t address, float *temp);

void um_onewire_update_state(uint64_t address, float temp);