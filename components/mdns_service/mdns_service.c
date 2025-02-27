#include "esp_log.h"
#include "esp_err.h"
#include "lwip/apps/netbiosns.h"
#include "mdns_service.h"
#include "../nvs/nvs.h"
#include "mdns.h"
#include "esp_log.h"

const char *MDNS_TAG = "mdns";

void um_mdns_prepare(void)
{
    char *hostname = um_nvs_read_str(NVS_KEY_HOSTNAME);

    ESP_ERROR_CHECK(mdns_init());

    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));

    ESP_LOGI(MDNS_TAG, "mdns hostname set to: [%s]", hostname);
    // set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP webserver"));

    // initialize service
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host("ESP32-WebServer", "_http", "_tcp", NULL, "_server"));

    free(hostname);
}
