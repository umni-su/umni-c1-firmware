#include "nvs_defines.h"

typedef struct
{
    char *inst;
    char *hane;
    char *admuser;
    char *admpwd
} umni_nvs_app_t;

typedef struct
{
    char *stname;
    char *stpwd;
    int wt;
    char *wip;
    char *wnm;
    char *wgw;
    char *wdns;

} um_nvs_wifi_t;

typedef struct
{
    /* data */
} umni_nvs_eth_t;

/**
 * Init nvs
 */
esp_err_t um_nvs_init();

/**
 * [open_nvs description]
 *
 * @param   char  namespace  [namespace description]
 *
 * @return  bool             [return description]
 */
bool um_nvs_open(char *namespace);

/**
 * [read_nvs_i8 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
int8_t um_nvs_read_i8(char *key);

/**
 * [read_nvs_i16 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
esp_err_t um_nvs_read_u16(char *key, uint16_t *out);

/**
 * Write uint16_t value to NVS
 *
 * @param   char      key  [key description]
 *
 * @return  uint16_t       [return description]
 */
esp_err_t um_nvs_write_u16(char *key, uint16_t value);

/**
 * [read_nvs_i64 description]
 *
 * @param   char     key  [key description]
 *
 * @return  int64_t       [return description]
 */
int64_t um_nvs_read_i64(char *key);

/**
 * [read_nvs_str description]
 *
 * @param   char  key  [key description]
 *
 * @return  char       [return description]
 */
char *um_nvs_read_str(char *key);

/**
 * [write_nvs_value_str description]
 *
 * @param   char  key    [key description]
 * @param   char  value  [value description]
 *
 * @return  bool         [return description]
 */
bool um_nvs_write_str(char *key, char *value);

/**
 * [write_nvs_value_i8 description]
 *
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
esp_err_t um_nvs_write_i8(char *key, int8_t value);

/**
 * [write_nvs_value_i64 description]
 *
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
esp_err_t um_nvs_write_value_i64(char *key, int64_t value);

/**
 * Get wifi sta ssid
 */
char *um_nvs_get_wifi_sta_ssid();

/** Set wifi sta ssid */
bool um_nvs_set_wifi_sta_ssid(char *sta_ssid);

/**
 * Get wifi sta password
 */
char *um_nvs_get_wifi_sta_pwd();

/**
 * Set wifi sta password
 */
bool um_nvs_set_wifi_sta_pwd(char *sta_pwd);

/**
 * Erease all dat in nvs
 */
bool um_nvs_erase();

/**
 * Delete nvs value by key
 */
bool um_nvs_delete_key(char *key);

/**
 * Close nvs
 */
void um_nvs_close();