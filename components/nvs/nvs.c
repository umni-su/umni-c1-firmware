#include "../events/event_types.h" // must be before esp_event.h
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_log.h"
#include "nvs.h"

const char *NVS_TAG = "nvs";
char *nvs_namespace = NULL;

nvs_handle_t my_nvs_handle;

esp_err_t um_nvs_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Fail initialize NVS srorage with code: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
    return err;
}

/**
 * Open NVS namespace
 */
bool um_nvs_open(char *namespace)
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
 * [read_nvs_i8 description]
 *
 * @param   char    key  [key description]
 *
 * @return  int8_t       [return description]
 */
int8_t um_nvs_read_i8(char *key)
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

esp_err_t um_nvs_read_u16(char *key, uint16_t *out)
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

esp_err_t um_nvs_write_u16(char *key, uint16_t value)
{
    esp_err_t err = nvs_set_u16(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %d", key, value);
    }
    return err;
}

int64_t um_nvs_read_i64(char *key)
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
char *um_nvs_read_str(char *key)
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
bool um_nvs_write_str(char *key, char *value)
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
esp_err_t um_nvs_write_i8(char *key, int8_t value)
{
    esp_err_t err = nvs_set_i8(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %d", key, value);
    }
    return err;
}

esp_err_t um_nvs_write_value_i64(char *key, int64_t value)
{
    esp_err_t err = nvs_set_i64(my_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error write NVS value. Namespace: %s, value: %lld", key, value);
    }
    return err;
}

char *um_nvs_get_wifi_sta_ssid()
{
    return read_nvs_str(NVS_KEY_WIFI_STA_SSID);
}

bool um_nvs_set_wifi_sta_ssid(char *sta_ssid)
{
    return write_nvs_value_str(NVS_KEY_WIFI_STA_SSID, sta_ssid);
}

char *um_nvs_get_wifi_sta_pwd()
{
    return read_nvs_str(NVS_KEY_WIFI_STA_PWD);
}

bool um_nvs_set_wifi_sta_pwd(char *sta_pwd)
{
    return write_nvs_value_str(NVS_KEY_WIFI_STA_PWD, sta_pwd);
}

/**
 * Delete key-value pair from NVS
 *
 * @param   char  key  [key description]
 *
 * @return  bool       [return description]
 */
bool um_nvs_delete_key(char *key)
{
    esp_err_t err = nvs_erase_key(my_nvs_handle, key);
    return err == ESP_OK;
}

/**
 * Erase NVS
 *
 * @return  bool    [return description]
 */
bool um_nvs_erase()
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
void um_nvs_close()
{
    nvs_close(my_nvs_handle);
}