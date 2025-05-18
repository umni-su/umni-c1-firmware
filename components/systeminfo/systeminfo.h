#include "esp_mac.h"
typedef struct
{
    char *name;
    char *mac;
    char *ip;
    char *mask;
    char *gw;

} um_netif_data_type_t;

typedef struct
{
    char *name;
    char *date;
    char *last_reset;
    uint64_t uptime; // microseconds
    esp_reset_reason_t restart_reason;
    uint32_t free_heap;
    size_t total_heap;
    int chip;
    int cores;
    int model;
    char *idf_ver;
    char *fw_ver;
    um_netif_data_type_t ip_eth_info;
    um_netif_data_type_t ip_wifi_info;

} um_systeminfo_data_type_t;

void um_systeminfo_init();

void um_systeminfo_task(void *arg);

void um_systeminfo_update_date();

char *um_systeminfo_get_date();

void um_systeminfo_init_sntp();

um_netif_data_type_t um_systeminfo_get_eth_netif_config();

um_systeminfo_data_type_t um_systeminfo_get_struct_data();

void um_systeminfo_uptime_to_string(char *str);