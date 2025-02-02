#include "nvs_defines.h"

typedef struct
{
    int inst;
    char *hane;
    char *admuser;
    char *admpwd;
    char *ntp;
    char *tz;
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
    int et;
    char *eip;
    char *enm;
    char *egw;
    char *edns;

} um_nvs_eth_t;

typedef struct
{
    int otdhwsp;
    int ottbsp;
} umni_nvs_ot_t;

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
 * Initialize NVS with basic values
 */
esp_err_t um_nvs_initialize_with_basic();

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
 * [um_nvs_read_str description]
 *
 * @param   char  key  [key description]
 *
 * @return  char       [return description]
 */
char *um_nvs_read_str(char *key);

/**
 * [um_nvs_write_str description]
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
 * Delete nvs value by key
 */
bool um_nvs_delete_key(char *key);

/**
 * Erease all dat in nvs
 */
bool um_nvs_erase();

/**
 * Close nvs
 */
void um_nvs_close();

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

bool um_nvs_is_installed();