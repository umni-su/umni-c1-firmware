#include "cJSON.h"
#define CONFIG_PATH "/cnf"
#define LOG_PATH "/log"
#define CONFIG_FILE_SENSORS "/dio.json"

esp_err_t um_config_init();

char *um_config_get_config_file(char *config_name);
bool um_config_delete_config_file(char *config_name);
bool um_config_write_config_file(char *config_name, cJSON *json);
void um_config_create_config_file_sensors();
char *um_config_get_text_from_file(FILE *file);
long um_config_get_file_size(FILE *file);

char *um_config_get_config_file_dio();