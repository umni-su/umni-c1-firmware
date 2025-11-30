#include <inttypes.h>
#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <pcf8574.h>
#include <string.h>

#include "includes/events.h"

#include "esp_event.h"
#include "esp_netif.h"

#include "../components/sd/sd.h"
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
#include "../components/opentherm/opentherm_operations.h"
#include "../components/rf433/rf433.h"

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static bool webserver_started = false;
static bool mqtt_connected = false;

TaskHandle_t sensors_task_handle = NULL;

void read_sensors(void *args)
{
    while (true)
    {
        ESP_LOGW("SENSORS", "Start reading sensors");
        onewire_task();
        ntc_queue_task();
        ai_queue_task();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

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

            //  Инициализируем входы при инициализации NVS
            //  чтобы обеспечить необходимый уровень при включении
            ESP_ERROR_CHECK(init_do());
            ESP_ERROR_CHECK(init_di());

            webserver_start();
            xTaskCreatePinnedToCore(read_sensors, "read_sensors", 4096, NULL, 3, &sensors_task_handle, 1);
            um_ot_init();
            break;
        case EV_NVS_OPENED:
            ethernet_start();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            um_sdcard_init();
            // Инициализируем SD карту
            while (!um_sd_card_detected())
            {
                ESP_LOGI("SD", "Trying to mount SD card");
                um_sd_mount();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            break;

        case EV_SYSTEM_INSTALLED:
            do_blink_led_stat_start_working();
            um_systeminfo_init();
            um_ota_mark_valid();
            break;

        case EV_NTP_SYNC_SUCCESS:
            ESP_LOGI(TAG, "SNTP syncronization success!");
            ESP_LOGI(TAG, "The current date/time is: %s", (char *)event_data);
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
        default:
            break;
        }
    }
}

static void um_alarm_interrupt_task(void *arg)
{
    int level = gpio_get_level(GPIO_NUM_13);
    if (level == 0)
    {
        ESP_LOGI(TAG, "ALARM OFF %d", level);
    }
    else
    {
        ESP_LOGW(TAG, "ALARM ON %d", level);
    }
    vTaskDelete(NULL);
}

static void IRAM_ATTR um_catch_alarm_interrupts(void *args)
{
    xTaskCreate(um_alarm_interrupt_task, "sd_cd_interrupt_task", 2096, NULL, 2, NULL);
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

    gpio_reset_pin(GPIO_NUM_13);
    gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_FLOATING);
    gpio_isr_handler_add(GPIO_NUM_13, um_catch_alarm_interrupts, NULL);
    gpio_set_intr_type(GPIO_NUM_13, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(GPIO_NUM_13);

    gpio_reset_pin(GPIO_NUM_12);
    gpio_set_direction(GPIO_NUM_12, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_FLOATING);
    gpio_set_level(GPIO_NUM_12, 1);

    gpio_reset_pin(GPIO_NUM_14);
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_NUM_14, GPIO_FLOATING);
    gpio_set_level(GPIO_NUM_14, 1);

    gpio_reset_pin(GPIO_NUM_15);
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_FLOATING);
    gpio_set_level(GPIO_NUM_15, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_15, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_15, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_15, 0);
}
