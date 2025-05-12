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
#include "../components/automation/automation.h"
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include "../components/opentherm/opentherm_operations.h"
#include "../components/rf433/rf433.h"

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static bool webserver_started = false;
static bool mqtt_connected = false;

void watch_any_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if ((int)id != 6)
    { // 6 == webserver read chunk
        // ESP_LOGI(TAG, "EVENT IS %08lX, %d", id, (int)id);
        switch (id)
        {
        case EV_SDCARD_MOUNTED:
            um_config_init();
            um_rf_433_init();
            init_adc();
            um_onewire_init();
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
            um_ot_init();
            break;

        case EV_SYSTEM_INSTALLED:
            do_blink_led_stat_start_working();
            um_systeminfo_init();
            um_ota_mark_valid();
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
        case EV_AUTOMATION_FIRED:
            um_am_main_t *automation = (um_am_main_t *)event_data;
            // int *on = automation->opts.relay_action.on;
            // int *off = automation->opts.relay_action.off;
            int on[6];
            int off[6];
            for (int i = 0; i < 6; i++)
            {
                on[i] = automation->opts.relay_action.on[i];
                off[i] = automation->opts.relay_action.off[i];
            }
            // DO
            for (int i = 0; i < 6; i++)
            {
                unsigned short int state;
                int prev_level;
                int real_channel;

                if (on[i] != -1)
                {
                    state = automation->inverted ? DO_HIGH : DO_LOW;
                    real_channel = do_map_channel(on[i]);
                    prev_level = do_get_level(real_channel);
                    // TODO PREV STATE and not switch case eq
                    // on[i] === channel index
                    if (prev_level != state)
                    {
                        do_set_level(real_channel, state); // state NOT invert, case ON[]
                        ESP_LOGW("automations", "\r\n[NORMAL]Trigger: %d, value: %0.1f, inv:%d", (char)automation->trigger.cond, automation->trigger.value, automation->inverted);
                        for (int i = 0; i < 6; i++)
                        {
                            if (automation->opts.relay_action.on[i] != -1)
                            {
                                ESP_LOGW("FIRE_AUTOMATION", "[OFF]Toggle relay i:%d state %s", i, state ? "ON" : "OFF");
                            }
                        }
                        ESP_LOGW("automations", "Boiler: %d\r\n", automation->opts.boiler_action.ch);
                    }
                }
                if (off[i] != -1)
                {
                    state = !automation->inverted ? DO_HIGH : DO_LOW;
                    real_channel = do_map_channel(off[i]);
                    prev_level = do_get_level(real_channel);
                    // on[i] === channel index
                    if (prev_level != state)
                    {
                        do_set_level(real_channel, state); // !state - invert, case OFF[]
                        ESP_LOGW("automations", "\r\n[INVERSE] Trigger: %d, value: %0.1f, inv:%d", (char)automation->trigger.cond, automation->trigger.value, automation->inverted);
                        for (int i = 0; i < 6; i++)
                        {
                            if (automation->opts.relay_action.off[i] != -1)
                            {
                                ESP_LOGW("FIRE_AUTOMATION", "[OFF]Toggle relay i:%d state %s", i, state ? "ON" : "OFF");
                            }
                        }
                        ESP_LOGW("automations", "Boiler: %d\r\n", automation->opts.boiler_action.ch);
                    }
                }
            }
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
