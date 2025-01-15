#include "../events/event_types.h" // must be before esp_event.h
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_log.h"
#include "nvs.h"

const char *NVS_TAG = "nvs";
char *nvs_namespace = NULL;

nvs_handle_t my_nvs_handle;

esp_err_t init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err != ESP_OK){
        ESP_LOGE(NVS_TAG,"Fail initialize NVS srorage with code: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
    return err;
}

/**
 * Initialize NVS on boot, fill required values in system
 *
 * @return  void    [return description]
 */
void fill_nvs_with_default_values()
{
    create_wifi_mode();

    int8_t val = read_nvs_i8(NVS_KEY_WIFI_MODE);
    esp_event_post(APP_EVENTS, UE_NVS_INITIALIZED, &val, sizeof(val), portMAX_DELAY);

    // Internal MCP23017
    uint16_t mcp_pins = 0;
    esp_err_t err = read_nvs_u16(NVS_KEY_MCP23017, &mcp_pins);
    if (err == ESP_FAIL)
    {
        write_nvs_u16(NVS_KEY_MCP23017, mcp_pins);
    }
}

/**
 * Open NVS namespace
 */
bool open_nvs(char *namespace)
{
    nvs_namespace = namespace;
    esp_err_t err = nvs_open(namespace, NVS_READWRITE, &my_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(NVS_TAG, "=================");
        ESP_LOGI(NVS_TAG, "Opening Non-Volatile Storage (NVS) handle OK ");
        ESP_LOGI(NVS_TAG, "=================");
        return true;
    }
    return false;
}

/**
 * [find_nvs_entry description]
 *
 * @param   char       it  [it description]
 *
 * @return  esp_err_t      [return description]
 */
esp_err_t find_nvs_entry(char *key, nvs_iterator_t it)
{
    if (nvs_namespace != NULL)
    {
        return nvs_entry_find(key, nvs_namespace, NVS_TYPE_ANY, &it);
    }
    return ESP_FAIL;
}

/**
 * [read_nvs_i8 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
int8_t read_nvs_i8(char *key)
{
    int8_t out_value;
    esp_err_t read_err = nvs_get_i8(my_nvs_handle, key, &out_value);
    if (read_err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Key %s not found. esp_err_t %s", key, esp_err_to_name(read_err));
        return -1;
    }
    else
    {
        ESP_LOGI(NVS_TAG, "Read (int8_t) KEY: %s, VAL: %d", key, out_value);
    }
    return out_value;
}

esp_err_t read_nvs_u16(char *key, uint16_t *out)
{
    esp_err_t read_err = nvs_get_u16(my_nvs_handle, key, out);
    if (read_err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Key %s not found. esp_err_t %s", key, esp_err_to_name(read_err));
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(NVS_TAG, "Read (int16_t) KEY: %s, VAL: %d", key, *(uint16_t *)out);
    }
    return ESP_OK;
}

esp_err_t write_nvs_u16(char *key, uint16_t value)
{
    esp_err_t err = nvs_set_u16(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %d", key, value);
    }
    return err;
}

int64_t read_nvs_i64(char *key)
{
    int64_t out_value;
    esp_err_t read_err = nvs_get_i64(my_nvs_handle, key, &out_value);
    if (read_err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Key %s not found. esp_err_t %s", key, esp_err_to_name(read_err));
        return -1;
    }
    else
    {
        ESP_LOGI(NVS_TAG, "Read (int64_t) KEY: %s, VAL: %lld", key, out_value);
    }
    return out_value;
}

/**
 * [read_nvs_str description]
 *
 * @param   char  key   [key description]
 * @param   int   type  [type description]
 *
 * @return  char        [return description]
 */
char *read_nvs_str(char *key)
{
    size_t required_size;
    char *val = NULL;
    esp_err_t read_err = nvs_get_str(my_nvs_handle, key, NULL, &required_size);
    if (read_err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(NVS_TAG, "Key %s not found", key);
        return NULL;
    }
    else if (read_err == ESP_FAIL)
    {
        ESP_LOGE(NVS_TAG, "An internal error; most likely due to corrupted NVS partition");
        return NULL;
    }
    else
    {
        val = malloc(required_size);
        nvs_get_str(my_nvs_handle, key, val, &required_size);
        ESP_LOGI(NVS_TAG, "Read (char *)) KEY: %s, VAL: %s", key, val);
    }
    return val;
}

/**
 * Write string value to NVS
 *
 * @param   char       key    [key description]
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
bool write_nvs_value_str(char *key, char *value)
{
    esp_err_t err = nvs_set_str(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %s", key, value);
    }
    return err == ESP_OK;
}

/**
 * Write int8_t value to NVS
 *
 * @param   char       value  [value description]
 *
 * @return  esp_err_t         [return description]
 */
esp_err_t write_nvs_value_i8(char *key, int8_t value)
{
    esp_err_t err = nvs_set_i8(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %d", key, value);
    }
    return err;
}

esp_err_t write_nvs_value_i64(char *key, int64_t value)
{
    esp_err_t err = nvs_set_i64(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %lld", key, value);
    }
    return err;
}

/**
 * Get NVS_WIFI_MODE in NVS
 *
 * @return  int8_t  [return description]
 */
int8_t get_wifi_mode()
{
    return read_nvs_i8(NVS_KEY_WIFI_MODE);
}

/**
 * Get NVS_WIFI_MODE in NVS
 *
 * @return  int8_t  [return description]
 */
char *get_wifi_ap_ssid()
{
    return read_nvs_str(NVS_KEY_WIFI_AP_SSID);
}

bool set_wifi_ap_ssid(char *ap_ssid)
{
    return write_nvs_value_str(NVS_KEY_WIFI_AP_SSID, ap_ssid);
}

char *get_wifi_ap_pwd()
{
    return read_nvs_str(NVS_KEY_WIFI_AP_PWD);
}

bool set_wifi_ap_pwd(char *ap_pwd)
{
    return write_nvs_value_str(NVS_KEY_WIFI_AP_PWD, ap_pwd);
}

esp_err_t delete_wifi_ap_pwd()
{
    return delete_nvs_key(NVS_KEY_WIFI_AP_PWD);
}

int8_t get_wifi_ap_channel()
{
    return read_nvs_i8(NVS_KEY_WIFI_AP_CHANNEL);
}

bool set_wifi_ap_channel(int8_t ap_channel)
{
    return write_nvs_value_i8(NVS_KEY_WIFI_AP_CHANNEL, ap_channel);
}

int8_t get_wifi_ap_max_connections()
{
    return read_nvs_i8(NVS_KEY_WIFI_AP_MAX_CONNECTIONS);
}

bool set_wifi_ap_max_connections(int8_t ap_max_connections)
{
    return write_nvs_value_i8(NVS_KEY_WIFI_AP_MAX_CONNECTIONS, ap_max_connections);
}

char *get_wifi_sta_ssid()
{
    return read_nvs_str(NVS_KEY_WIFI_STA_SSID);
}

bool set_wifi_sta_ssid(char *sta_ssid)
{
    return write_nvs_value_str(NVS_KEY_WIFI_STA_SSID, sta_ssid);
}

char *get_wifi_sta_pwd()
{
    return read_nvs_str(NVS_KEY_WIFI_STA_PWD);
}

bool set_wifi_sta_pwd(char *sta_pwd)
{
    return write_nvs_value_str(NVS_KEY_WIFI_STA_PWD, sta_pwd);
}

/**
 * Update NVS_WIFI_MODE in NVS
 *
 * @return  int8_t  [return description]
 */
int8_t update_wifi_mode(int8_t mode)
{
    esp_err_t wifi_mode = write_nvs_value_i8(NVS_KEY_WIFI_MODE, mode);
    if (wifi_mode == ESP_OK)
    {
        esp_event_post(APP_EVENTS, UE_WIFI_MODE_UPDATED, &mode, sizeof(mode), portMAX_DELAY);
    }
    return wifi_mode;
}

/**
 * Write AP mode to NVS if not exists
 *
 * @return  int8_t  [return description]
 */
int8_t create_wifi_mode()
{
    int8_t val = read_nvs_i8(NVS_KEY_WIFI_MODE);
    if (val < 0)
    {
        update_wifi_mode(NVS_WIFI_MODE_STA);
    }
    return val;
}

/**
 * Delete key-value pair from NVS
 *
 * @param   char  key  [key description]
 *
 * @return  bool       [return description]
 */
bool delete_nvs_key(char *key)
{
    esp_err_t err = nvs_erase_key(my_nvs_handle, key);
    return err == ESP_OK;
}

/**
 * Erase NVS
 *
 * @return  bool    [return description]
 */
bool erase_nvs()
{
    esp_err_t err = nvs_erase_all(my_nvs_handle);
    if (err == ESP_OK)
    {
        return true;
    }
    else
    {
        ESP_LOGE(NVS_TAG, "!!! Error erase NVS");
        return false;
    }
}

/**
 * Close NVS namespace
 *
 * @return  void    [return description]
 */
void close_nvs()
{
    nvs_close(my_nvs_handle);
}