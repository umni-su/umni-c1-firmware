#include <inttypes.h>
#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <pcf8574.h>
#include <string.h>

#include "includes/events.h"

#include "esp_event.h"
#include "esp_netif.h"

#include "../components/ethernet/ethernet.h"
#include "../components/webserver/webserver.h"
#include "../components/nvs/nvs.h"
#include "../components/adc/adc.h"
#include "../components/dio/dio.h"
#include "../components/1wire/1wire.h"
#include "../components/systeminfo/systeminfo.h"
#include "../components/config/config.h"
#include "../components/mosquitto/mosquitto.h"
#include "../components/ota/ota.h"
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include "../components/opentherm/opentherm_operations.h"

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static i2c_dev_t pcf8574_inp;

static TaskHandle_t do_handle;
static TaskHandle_t di_handle;

static bool webserver_started = false;
static bool mqtt_connected = false;

void watch_any_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if ((int)id != 6)
    { // 6 == webserver read chunk
        ESP_LOGI(TAG, "EVENT IS %08lX, %d", id, (int)id);
        switch (id)
        {
        case EV_SDCARD_MOUNTED:
            um_config_init();
            break;
        case EV_CONFIGURATION_READY:
            webserver_start();
            //  Инициализируем входы при инициализации NVS
            //  чтобы обеспечить необходимый уровень при включении
            ESP_ERROR_CHECK(init_do());
            ESP_ERROR_CHECK(init_di());
            break;
        case EV_NVS_OPENED:
            ethernet_start();
            break;
        case EV_SYSTEM_INSTALLED:

            do_blink_led_stat_start_working();
            um_systeminfo_init();
            init_opentherm();
            init_adc();
            onewire_init_config();
            break;

        case EV_NTP_SYNC_SUCCESS:
            ESP_LOGI(TAG, "SNTP syncronization success!");
            ESP_LOGI(TAG, "The current date/time in Moscow is: %s", (char *)event_data);
            break;

        case EV_ETH_MAC:
            char *mac = (char *)event_data;
            ESP_LOGI(TAG, "Save ETH MAC to NVS %s", mac);
            um_nvs_write_str(NVS_KEY_ETH_MAC, mac);
            break;
        case EV_ETH_GOT_IP:
            do_blink_led_stat_stop();
            ESP_LOGI(TAG, "Ethernet got ip success!");
            do_blink_led_stat_start_working();
            um_ota_mark_valid();
            break;

        case IP_EVENT_ETH_GOT_IP | IP_EVENT_STA_GOT_IP:
            // Start webserver ONCE
            if (!webserver_started)
            {
                webserver_started = true;
            }
            if (!mqtt_connected)
            {
                mqtt_connected = true;
                um_mqtt_init();
            }
            break;

        case ETHERNET_EVENT_CONNECTED:
            do_blink_led_err_stop();
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            do_blink_led_error();
            break;

        case EV_OTA_START:
            do_blink_led_stat_start_flashing();
            break;
        case EV_OTA_SUCCESS:
            esp_restart();
            break;
        case EV_OTA_ABORT:
            do_blink_led_stat_start_working();
            break;
        default:
            break;
        }
    }
}

void app_main(void)
{

    ESP_LOGI(TAG, "\r\nStarting load UMNI firmware at version: %s \r\n", CONFIG_APP_PROJECT_VER);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &watch_any_event, NULL));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(um_nvs_init());
}
