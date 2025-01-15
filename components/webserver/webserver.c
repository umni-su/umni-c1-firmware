/* HTTP Restful API Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include "esp_vfs_semihost.h"
#include "lwip/apps/netbiosns.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"


#if SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#endif
#include "sdmmc_cmd.h"

static const char *WEBSERVER_TAG = "webserver";

/* IMPORT METHODS FROM REST_SERVER */
//esp_err_t start_rest_server(const char *base_path);

/* END IMPORT METHODS FROM REST_SERVER */

static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(WEBSERVER_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(WEBSERVER_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(WEBSERVER_TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(WEBSERVER_TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(WEBSERVER_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[300];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(WEBSERVER_TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

#if CONFIG_UMNI_WEB_DEPLOY_SEMIHOST
esp_err_t init_fs(void)
{
    esp_err_t ret = esp_vfs_semihost_register(CONFIG_UMNI_WEB_MOUNT_POINT);
    if (ret != ESP_OK) {
        ESP_LOGE(WEBSERVER_TAG, "Failed to register semihost driver (%s)!", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif

#if CONFIG_UMNI_WEB_DEPLOY_SD

esp_err_t init_fs(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t* card;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.gpio_cs = GPIO_NUM_17;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    slot_config.host_id = CONFIG_UMNI_ETH_SPI_HOST;
    //ret = spi_bus_add_device(CONFIG_UMNI_ETH_SPI_HOST,  &buscfg, NULL);
    ESP_ERROR_CHECK(esp_vfs_fat_sdspi_mount(CONFIG_UMNI_WEB_MOUNT_POINT, &host, &slot_config, &mount_config, &card));
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

#endif

void webserver_start_task(void *args){
    // ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(init_fs());
    //ESP_ERROR_CHECK(start_rest_server(CONFIG_UMNI_WEB_MOUNT_POINT"/www"));
    vTaskDelete(NULL);
}

/**
 * Webserver start task
 */
void webserver_start()
{
    xTaskCreate(webserver_start_task, "init_webserver", 4095, NULL, 10, NULL);
}