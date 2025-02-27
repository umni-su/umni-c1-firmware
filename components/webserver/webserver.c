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
#include "esp_log.h"

#include "../config/config.h"

static const char *WEBSERVER_TAG = "webserver";

/* IMPORT METHODS FROM REST_SERVER */
esp_err_t start_rest_server(const char *base_path);

void um_mdns_prepare(void);

/* END IMPORT METHODS FROM REST_SERVER */

void webserver_start_task(void *args)
{
    ESP_LOGI(WEBSERVER_TAG, "Start web server");
    // ESP_ERROR_CHECK(esp_netif_init());
// ESP_ERROR_CHECK(start_rest_server(CONFIG_UMNI_SD_MOUNT_POINT CONFIG_UMNI_WEB_MOUNT_POINT));
#if CONFIG_UMNI_WEB_DEPLOY_SD
    ESP_ERROR_CHECK(start_rest_server(CONFIG_UMNI_SD_MOUNT_POINT CONFIG_UMNI_WEB_MOUNT_POINT));
#else if CONFIG_UMNI_WEB_DEPLOY_SEMIHOST
    ESP_ERROR_CHECK(start_rest_server("/www"));
#endif
    //   ESP_ERROR_CHECK(start_rest_server(CONFIG_UMNI_WEB_MOUNT_POINT"/www"));
    vTaskDelete(NULL);
}

/**
 * Webserver start task
 */
void webserver_start()
{
    xTaskCreate(webserver_start_task, "init_webserver", 4095, NULL, 10, NULL);
}