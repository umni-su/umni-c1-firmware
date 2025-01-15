#include "nvs_defines.h"

/**
 * Inialize nvs with basic values
 */
void fill_nvs_with_default_values();

/**
 * Init nvs
*/
esp_err_t init_nvs();

/**
 * [open_nvs description]
 *
 * @param   char  namespace  [namespace description]
 *
 * @return  bool             [return description]
 */
bool open_nvs(char *namespace);

/**
 * [read_nvs_i8 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
int8_t read_nvs_i8(char *key);

/**
 * [read_nvs_i16 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
esp_err_t read_nvs_u16(char *key, uint16_t *out);

/**
 * Write uint16_t value to NVS
 *
 * @param   char      key  [key description]
 *
 * @return  uint16_t       [return description]
 */
esp_err_t write_nvs_u16(char *key, uint16_t value);

/**
 * [read_nvs_i64 description]
 *
 * @param   char     key  [key description]
 *
 * @return  int64_t       [return description]
 */
int64_t read_nvs_i64(char *key);

/**
 * [read_nvs_str description]
 *
 * @param   char  key  [key description]
 *
 * @return  char       [return description]
 */
char *read_nvs_str(char *key);

/**
 * [write_nvs_value_str description]
 *
 * @param   char  key    [key description]
 * @param   char  value  [value description]
 *
 * @return  bool         [return description]
 */
bool write_nvs_value_str(char *key, char *value);

/**
 * [write_nvs_value_i8 description]
 *
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
esp_err_t write_nvs_value_i8(char *key, int8_t value);

/**
 * [write_nvs_value_i64 description]
 *
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
esp_err_t write_nvs_value_i64(char *key, int64_t value);

/**
 * Get wifi-mode from nvs
 */
int8_t get_wifi_mode();

/**
 * Update wifi mode in nvs
 * @param int8_t mode
 */
int8_t update_wifi_mode(int8_t mode);

/**
 * Initialize wifi mode
 */
int8_t create_wifi_mode();

/**
 * Get wifi ap ssid
 */
char *get_wifi_ap_ssid();

/**
 * Set wifi ap ssid to nvs
 */
bool set_wifi_ap_ssid(char *ap_ssid);

/**
 * Get wifi ap password
 */
char *get_wifi_ap_pwd();

/**
 * Set wifi ap password to nvs
 */
bool set_wifi_ap_pwd(char *ap_pwd);

/**
 * Delete wifi ap password
 */
esp_err_t delete_wifi_ap_pwd();

/**
 * Get wifi ap channel
 */
int8_t get_wifi_ap_channel();

/**
 * Set wifi ap channel
 */
bool set_wifi_ap_channel(int8_t ap_channel);

/**
 * Get fifi ap max connections
 */
int8_t get_wifi_ap_max_connections();

/**
 * Set wifi ap max connections
 */
bool set_wifi_ap_max_connections(int8_t ap_max_connections);

/**
 * Get wifi sta ssid
 */
char *get_wifi_sta_ssid();

/** Set wifi sta ssid */
bool set_wifi_sta_ssid(char *sta_ssid);

/**
 * Get wifi sta password
 */
char *get_wifi_sta_pwd();

/**
 * Set wifi sta password
 */
bool set_wifi_sta_pwd(char *sta_pwd);

/**
 * Erease all dat in nvs
 */
bool erase_nvs();

/**
 * Delete nvs value by key
 */
bool delete_nvs_key(char *key);

/**
 * Close nvs
 */
void close_nvs();