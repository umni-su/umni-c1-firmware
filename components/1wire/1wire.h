#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "onewire.h"
#include "ds18x20.h"

#define ONEWIRE_TASK_TIMEOUT 20000

void onewire_configure();
void onewire_task(void *arg);
void onewire_init_config();
void onewire_addr_str_to_uint64_t(char *address, uint64_t *out);
void onewire_uint64_t_to_addr_str(onewire_addr_t address, char *out);
esp_err_t ds18x20_read_temp(onewire_addr_t address, float *temp);