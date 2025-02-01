#include "esp_log.h"
#include "opentherm.h"

#define OT_IN_PIN CONFIG_UMNI_OT_IN
#define OT_OUT_PIN CONFIG_UMNI_OT_OUT
#define ESP_INTR_FLAG_DEFAULT 0

volatile float dhwTemp = 0;
volatile float chTemp = 0;
volatile bool fault = false;

static int targetDHWTemp = 59;
static int targetCHTemp = 60;

bool enableCentralHeating = false;
bool enableHotWater = true;
bool enableCooling = false;
bool enableOutsideTemperatureCompensation = false;
bool enableCentralHeating2 = false;

static const char *TAG = "OPENTHERM";

void esp_ot_control_task_handler(void *pvParameter)
{
    if (!enableCentralHeating)
    {
        enableCentralHeating = true;
    }
    while (1)
    {
        unsigned long status = esp_ot_set_boiler_status(
            enableCentralHeating,
            enableHotWater, enableCooling,
            enableOutsideTemperatureCompensation,
            enableCentralHeating2);

        ESP_LOGI(TAG, "====== OPENTHERM =====");
        ESP_LOGI(TAG, "Free heap size before: %ld", esp_get_free_heap_size());
        open_therm_response_status_t esp_ot_response_status = esp_ot_get_last_response_status();
        if (esp_ot_response_status == OT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Central Heating: %s", esp_ot_is_central_heating_active(status) ? "ON" : "OFF");
            ESP_LOGI(TAG, "Hot Water: %s", esp_ot_is_hot_water_active(status) ? "ON" : "OFF");
            ESP_LOGI(TAG, "Flame: %s", esp_ot_is_flame_on(status) ? "ON" : "OFF");
            fault = esp_ot_is_fault(status);

            ESP_LOGI(TAG, "Fault: %s", fault ? "YES" : "NO");
            if (fault)
            {
                ot_reset();
            }
            esp_ot_set_boiler_temperature(targetCHTemp);
            ESP_LOGI(TAG, "Set CH Temp to: %i", targetCHTemp);

            esp_ot_set_dhw_setpoint(targetDHWTemp);
            ESP_LOGI(TAG, "Set DHW Temp to: %i", targetDHWTemp);

            dhwTemp = esp_ot_get_return_temperature();
            ESP_LOGI(TAG, "Tret: %.1f", dhwTemp);

            chTemp = esp_ot_get_boiler_temperature();
            ESP_LOGI(TAG, "CH Temp: %.1f", chTemp);

            float pressure = esp_ot_get_pressure();
            ESP_LOGI(TAG, "Slave OT Version: %.1f", pressure);

            unsigned long slaveProductVersion = esp_ot_get_slave_product_version();
            ESP_LOGI(TAG, "Slave Version: %08lX", slaveProductVersion);

            float slaveOTVersion = esp_ot_get_slave_ot_version();
            ESP_LOGI(TAG, "Slave OT Version: %.1f", slaveOTVersion);
        }
        else if (esp_ot_response_status == OT_STATUS_TIMEOUT)
        {
            ESP_LOGE(TAG, "OT Communication Timeout");
        }
        else if (esp_ot_response_status == OT_STATUS_INVALID)
        {
            ESP_LOGE(TAG, "OT Communication Invalid");
        }
        else if (esp_ot_response_status == OT_STATUS_NONE)
        {
            ESP_LOGE(TAG, "OpenTherm not initialized");
        }

        if (fault)
        {
            ESP_LOGE(TAG, "Fault Code: %i", esp_ot_get_fault());
        }
        ESP_LOGI(TAG, "Free heap size after: %ld", esp_get_free_heap_size());
        ESP_LOGI(TAG, "====== OPENTHERM =====\r\n\r\n");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void init_opentherm()
{
    esp_ot_init(
        OT_IN_PIN,
        OT_OUT_PIN,
        false,
        NULL);

    xTaskCreate(esp_ot_control_task_handler, TAG, configMINIMAL_STACK_SIZE * 4, NULL, 3, NULL);
}