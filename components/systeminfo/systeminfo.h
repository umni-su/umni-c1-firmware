typedef struct
{
    char *date;
    char *last_reset;
    char *uptime;
    esp_reset_reason_t restart_reason;
    uint32_t free_heap;
    int chip;
    int cores;
    int model;
    char *idf_ver;
    char *fw_ver;
    char *web_fw_ver;

} um_systeminfo_data_type_t;

void um_systeminfo_init();

void um_systeminfo_task(void *arg);

void um_systeminfo_update_date();

char *um_systeminfo_get_date();

void um_systeminfo_init_sntp();

um_systeminfo_data_type_t um_systeminfo_get_struct_data();
