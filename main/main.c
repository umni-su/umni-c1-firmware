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
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "../components/opentherm/opentherm_operations.h"

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static i2c_dev_t pcf8574_inp;

static TaskHandle_t do_handle;
static TaskHandle_t di_handle;

void watch_any_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if ((int)id != 6)
    { // 6 == webserver read chunk
        ESP_LOGI(TAG, "EVENT IS %08lX, %d", id, (int)id);
        switch (id)
        {
        case EV_NVS_OPENED:
            ethernet_start();
            vTaskDelay(pdMS_TO_TICKS(200));

            webserver_start();
            vTaskDelay(pdMS_TO_TICKS(200));
            // Инициализируем входы при инициализации NVS
            // чтобы обеспечить необходимый уровень при включении
            ESP_ERROR_CHECK(init_do());

            break;
        case EV_SYSTEM_INSTALLED:

            um_systeminfo_init();

            ESP_ERROR_CHECK(init_di());

            init_opentherm();

            init_adc();

            onewire_init_config();
            break;

        case EV_NTP_SYNC_SUCCESS:
            ESP_LOGI(TAG, "SNTP syncronization success!");
            ESP_LOGI(TAG, "The current date/time in Moscow is: %s", (char *)event_data);
            break;

        default:
            break;
        }
    }
}

void app_main(void)
{

    ESP_LOGI(TAG, "!Starting load UMNI firmware!");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &watch_any_event, NULL));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(um_nvs_init());
}
