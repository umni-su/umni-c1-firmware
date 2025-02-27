#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "esp_chip_info.h"

#include "../config.h"
#include "../nvs/nvs.h"
#include "../../main/includes/events.h"
#include "systeminfo.h"

const char *TAG = "systeminfo";
const char *ntp_host;
static char *reset_at = NULL;

time_t now;
char strftime_buf[64];
struct tm timeinfo;

TaskHandle_t systeminfo_task_handle = NULL;

um_netif_data_type_t eth_ip_info;

static um_systeminfo_data_type_t data;

static void
shutdown_handler()
{
    um_nvs_write_str(NVS_KEY_RESET_AT, um_systeminfo_get_date());
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    static char _ip[16];
    static char _mask[16];
    static char _gw[16];
    char *_mac = um_nvs_read_str(NVS_KEY_ETH_MAC);

    sprintf(_ip, "%d.%d.%d.%d", IP2STR(&ip_info->ip));
    sprintf(_mask, "%d.%d.%d.%d", IP2STR(&ip_info->netmask));
    sprintf(_gw, "%d.%d.%d.%d", IP2STR(&ip_info->gw));

    ESP_LOGI(TAG, "================");
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "IP:%s MASK:%s GW:%s MAC%s", _ip, _mask, _gw, _mac);
    ESP_LOGI(TAG, "================");
    eth_ip_info.name = "Ethernet";
    eth_ip_info.ip = _ip;
    eth_ip_info.mask = _mask;
    eth_ip_info.gw = _gw;
    eth_ip_info.mac = _mac;

    // Fire ethernet got ip event
    esp_event_post(APP_EVENTS, EV_ETH_GOT_IP, (void *)NULL, sizeof(NULL), portMAX_DELAY);
}

void um_systeminfo_task(void *arg)
{
    um_systeminfo_get_date();
    esp_event_post(APP_EVENTS, EV_NTP_SYNC_SUCCESS, (void *)strftime_buf, sizeof(strftime_buf), portMAX_DELAY);
    vTaskDelete(systeminfo_task_handle);
}

void time_sync_notification_cb(struct timeval *tv)
{
    xTaskCreatePinnedToCore(um_systeminfo_task, "systeminfo", 4095, NULL, 10, &systeminfo_task_handle, 1);

    ESP_LOGW(TAG, "Notification of a time synchronization event");
    um_nvs_write_str(NVS_KEY_RESET_AT, um_systeminfo_get_date());
}

void um_systeminfo_init_sntp()
{
    ntp_host = um_nvs_read_str(NVS_KEY_NTP);

    char *tz = um_nvs_read_str(NVS_KEY_TIMEZONE);

    if (tz == NULL)
    {
        tz = "MSK-3";
        um_nvs_write_str(NVS_KEY_TIMEZONE, tz);
    }

    // configure the event on which we renew servers
    setenv("TZ", tz, 1);
    tzset();

    if (ntp_host != NULL)
    {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_host);
        config.start = false;                     // start SNTP service explicitly (after connecting)
        config.server_from_dhcp = false;          // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
        config.renew_servers_after_new_IP = true; // let esp-netif update configured SNTP server(s) after receiving DHCP lease
        config.index_of_first_server = 0;         // updates from server num 1, leaving server 0 (from DHCP) intact
        config.ip_event_to_renew = IP_EVENT_STA_GOT_IP | IP_EVENT_ETH_GOT_IP;
        config.sync_cb = time_sync_notification_cb;
        // esp_sntp_setservername(0, ntp_host);
        esp_netif_sntp_init(&config);
        esp_netif_sntp_start();
    }
}

void um_systeminfo_update_date()
{
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
}

char *um_systeminfo_get_date()
{
    um_systeminfo_update_date();
    return strftime_buf;
}

void um_systeminfo_init()
{
    um_nvs_write_str(NVS_KEY_POWERON_AT, strftime_buf);
    esp_register_shutdown_handler(shutdown_handler);
    um_systeminfo_init_sntp();
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
}

um_systeminfo_data_type_t um_systeminfo_get_struct_data()
{
    // Утечка?
    um_systeminfo_update_date();
    reset_at = um_nvs_read_str(NVS_KEY_RESET_AT);
    esp_chip_info_t info;
    esp_chip_info(&info);
    data.date = strftime_buf;
    data.last_reset = reset_at;
    data.uptime = esp_timer_get_time();
    data.restart_reason = esp_reset_reason();
    data.free_heap = esp_get_free_heap_size();
    data.total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    data.chip = (int)info.revision;
    data.cores = (int)info.cores;
    data.model = (int)info.model;
    data.idf_ver = IDF_VER;
    data.fw_ver = FW_VER;
    data.fw_ver_web = FW_VER_WEB;
    data.ip_eth_info.name = eth_ip_info.name;
    data.ip_eth_info.ip = eth_ip_info.ip;
    data.ip_eth_info.mask = eth_ip_info.mask;
    data.ip_eth_info.gw = eth_ip_info.gw;
    data.ip_eth_info.mac = eth_ip_info.mac;
    return data;
}