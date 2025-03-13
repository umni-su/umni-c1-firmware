#include "esp_log.h"
#include "opentherm.h"
#include "esp_event.h"
#include "opentherm_operations.h"

#include "../../main/includes/events.h"
#include "../nvs/nvs.h"

#define OT_IN_PIN 32
#define OT_OUT_PIN 33
#define ESP_INTR_FLAG_DEFAULT 0

static int targetDHWTemp = 59;
static int targetCHTemp = 60;
static bool fault = false;

bool enableCentralHeating = true;
bool enableHotWater = true;
bool enableCooling = false;
bool enableOutsideTemperatureCompensation = false;
bool enableCentralHeating2 = false;

static const char *TAG = "OPENTHERM";

static um_ot_data_t ot_data;

TaskHandle_t ot_handle = NULL;

open_therm_response_status_t ot_response_status;

bool is_busy = false;

unsigned long status;

void esp_ot_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id != EV_OT_CH_ON && id != EV_OT_CH_OFF)
    {
        return;
    }
    enableCentralHeating = id == EV_OT_CH_ON ? true : false;

    esp_err_t res = um_ot_set_boiler_status(
        enableCentralHeating,
        enableHotWater, enableCooling,
        enableOutsideTemperatureCompensation,
        enableCentralHeating2);
    ESP_LOGI(TAG, "OT CH triggered by event. OT is %s", enableCentralHeating ? "ON" : "OFF");
}

void esp_ot_control_task_handler(void *pvParameter)
{
    // Устанавливаем начальные целевые значения из NVS
    targetDHWTemp = um_nvs_read_i8(NVS_KEY_OT_DHW_SETPOINT);
    targetCHTemp = um_nvs_read_i8(NVS_KEY_OT_TB_SETPOINT);
    enableCentralHeating = um_nvs_read_i8(NVS_KEY_OT_ENABLED) == 1;

    while (true)
    {
        esp_err_t res = um_ot_set_boiler_status(
            enableCentralHeating,
            enableHotWater, enableCooling,
            enableOutsideTemperatureCompensation,
            enableCentralHeating2);
        um_ot_set_boiler_temp(targetCHTemp);
        um_ot_set_dhw_setpoint(targetDHWTemp);

        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Opentherm um_ot_set_boiler_status return ESP_FAIL, retry...");
            res = um_ot_set_boiler_status(
                enableCentralHeating,
                enableHotWater, enableCooling,
                enableOutsideTemperatureCompensation,
                enableCentralHeating2);
            um_ot_set_boiler_temp(targetCHTemp);
            um_ot_set_boiler_temp(targetDHWTemp);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        if (!is_busy)
        {
            is_busy = true;

            ESP_LOGI(TAG, "\r\n====== OPENTHERM DATA =====");
            ESP_LOGI(TAG, "Free heap size before: %ld", esp_get_free_heap_size());
            ESP_LOGI(TAG, "NVS OT values - ch: %d, dhwsp: %d tbsp: %d", enableCentralHeating, targetDHWTemp, targetCHTemp);

            ot_response_status = esp_ot_get_last_response_status();

            if (ot_response_status == OT_STATUS_SUCCESS)
            {
                const float modulation = esp_ot_get_modulation();
                const float pressure = esp_ot_get_pressure();
                const float slave_ot_version = esp_ot_get_slave_ot_version();
                const unsigned long slave_product_version = esp_ot_get_slave_product_version();
                const float dhwTemp = esp_ot_get_dhw_temperature();
                const float chTemp = esp_ot_get_boiler_temperature();
                const float retTemp = esp_ot_get_return_temperature();

                ot_data.status = ot_response_status;
                ot_data.otch = enableCentralHeating;

                ot_data.pressure = pressure;
                ot_data.modulation = modulation;
                ot_data.slave_ot_version = slave_ot_version;
                ot_data.slave_product_version = slave_product_version;
                ot_data.dhw_temperature = dhwTemp;
                ot_data.boiler_temperature = chTemp;
                ot_data.return_temperature = retTemp;
                ot_data.is_fault = fault;
                ot_data.otdhwsp = targetDHWTemp;
                ot_data.ottbsp = targetCHTemp;

                if (fault)
                {
                    ot_reset();
                }

                ESP_LOGI(TAG, "Central Heating: %s", ot_data.central_heating_active ? "ON" : "OFF");
                ESP_LOGI(TAG, "Hot Water: %s", ot_data.hot_water_active ? "ON" : "OFF");
                ESP_LOGI(TAG, "Flame: %s", ot_data.flame_on ? "ON" : "OFF");
                ESP_LOGI(TAG, "Fault: %s", fault ? "YES" : "NO");
                ESP_LOGI(TAG, "Tret: %.1f", dhwTemp);
                ESP_LOGI(TAG, "CH Temp: %.1f", chTemp);
                ESP_LOGI(TAG, "Pressure: %.1f", pressure);
                ESP_LOGI(TAG, "Modulation:%.1f", modulation);
                ESP_LOGI(TAG, "Slave OT Version: %.1f", slave_ot_version);
                ESP_LOGI(TAG, "Slave Version: %08lX", slave_product_version);
            }
            else
            {
                ESP_LOGW(TAG, "Error readingg %d", ot_response_status);
            }

            if (fault)
            {
                char code = esp_ot_get_fault();
                ot_data.fault_code = code;
                ESP_LOGE(TAG, "Fault Code: %i", code);
            }
            ESP_LOGI(TAG, "Free heap size after: %ld", esp_get_free_heap_size());
            ESP_LOGI(TAG, "====== OPENTHERM =====\r\n\r\n");
            is_busy = false;
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t um_ot_set_boiler_status(
    bool enable_central_heating,
    bool enable_hot_water,
    bool enable_cooling,
    bool enable_outside_temperature_compensation,
    bool enable_central_heating2)
{
    // vTaskSuspend(ot_handle);

    enableCentralHeating = enable_central_heating;
    enableHotWater = enable_hot_water;
    enableCooling = enable_cooling;
    enableOutsideTemperatureCompensation = enable_outside_temperature_compensation;
    enableCentralHeating2 = enable_central_heating2;

    um_nvs_write_i8(NVS_KEY_OT_ENABLED, (int)enableCentralHeating);
    ot_data.otch = (int)enableCentralHeating;

    esp_err_t res = ESP_OK;
    status = esp_ot_set_boiler_status(
        enableCentralHeating,
        enableHotWater,
        enableCooling,
        enableOutsideTemperatureCompensation,
        enableCentralHeating2);

    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        ot_data.central_heating_active = esp_ot_is_central_heating_active(status);
        ot_data.hot_water_active = esp_ot_is_hot_water_active(status);
        ot_data.flame_on = esp_ot_is_flame_on(status);
        ot_data.is_fault = esp_ot_is_fault(status);
    }
    else if (ot_response_status == OT_STATUS_TIMEOUT)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OT Communication Timeout");
    }
    else if (ot_response_status == OT_STATUS_INVALID)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OT Communication Invalid");
    }
    else if (ot_response_status == OT_STATUS_NONE)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OpenTherm not initialized");
    }
    else
    {
        res = ESP_FAIL;
    }

    // vTaskResume(ot_handle);

    return res;
}

// Установка целевой температуры бойлера
void um_ot_set_boiler_temp(float temp)
{
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        targetCHTemp = temp;
        um_nvs_write_i8(NVS_KEY_OT_TB_SETPOINT, targetCHTemp);
        esp_ot_set_boiler_temperature(targetCHTemp);
        ESP_LOGI(TAG, "Set CH Temp to: %i", targetCHTemp);
    }
}
// Установка целевой горячей воды (ГВС)
void um_ot_set_dhw_setpoint(float temp)
{
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        targetDHWTemp = temp;
        um_nvs_write_i8(NVS_KEY_OT_DHW_SETPOINT, temp);
        esp_ot_set_dhw_setpoint(targetDHWTemp);
        ESP_LOGI(TAG, "Set DHW Temp to: %i", targetDHWTemp);
    }
}

void um_ot_init()
{
    esp_event_handler_register(APP_EVENTS, ESP_EVENT_ANY_ID, &esp_ot_event_handler, NULL);

    esp_ot_init(
        OT_IN_PIN,
        OT_OUT_PIN,
        false,
        NULL);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Get or ser DHW sp;
    targetDHWTemp = um_nvs_read_i8(NVS_KEY_OT_DHW_SETPOINT);
    if (targetDHWTemp < 0)
    {
        targetDHWTemp = 45;
        um_nvs_write_i8(NVS_KEY_OT_TB_SETPOINT, targetDHWTemp);
    }

    // Get or ser TB sp;
    targetCHTemp = um_nvs_read_i8(NVS_KEY_OT_TB_SETPOINT);
    if (targetCHTemp < 0)
    {
        targetCHTemp = 60;
        um_nvs_write_i8(NVS_KEY_OT_TB_SETPOINT, targetCHTemp);
    }

    ot_data.otdhwsp = targetDHWTemp;
    ot_data.ottbsp = targetCHTemp;
    ot_data.otch = enableCentralHeating;

    xTaskCreatePinnedToCore(esp_ot_control_task_handler, TAG, configMINIMAL_STACK_SIZE * 4, NULL, 3, &ot_handle, 1);
}

um_ot_data_t um_ot_get_data()
{
    return ot_data;
}

void um_ot_update_state(bool otch, int otdhwsp, int ottbsp)
{
    if (otdhwsp > 60)
    {
        otdhwsp = 60;
    }

    if (ottbsp > 80)
    {
        ottbsp = 80;
    }
    targetDHWTemp = otdhwsp;
    targetCHTemp = ottbsp;
    enableCentralHeating = otch ? 1 : 0;
    ot_data.otdhwsp = targetDHWTemp;
    ot_data.ottbsp = targetCHTemp;
    // esp_ot_set_dhw_setpoint(otdhwsp);
    // esp_ot_set_boiler_temperature(ottbsp);

    // esp_ot_set_boiler_status(
    //     enableCentralHeating,
    //     enableHotWater, enableCooling,
    //     enableOutsideTemperatureCompensation,
    //     enableCentralHeating2);
    // um_nvs_write_i8(NVS_KEY_OT_DHW_SETPOINT, otdhwsp);
    // um_nvs_write_i8(NVS_KEY_OT_TB_SETPOINT, ottbsp);
    // um_nvs_write_i8(NVS_KEY_OT_ENABLED, otch);

    um_ot_set_boiler_temp(ottbsp);
    um_ot_set_dhw_setpoint(otdhwsp);

    esp_err_t res = um_ot_set_boiler_status(
        enableCentralHeating,
        enableHotWater, enableCooling,
        enableOutsideTemperatureCompensation,
        enableCentralHeating2);

    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "otch: %d  otdhwsp: %d ottbsp:%d", otch, otdhwsp, ottbsp);
    }
}