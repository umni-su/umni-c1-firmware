#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "1wire.h"
#include "../config/config.h"
// #include "../automation/um_automation.h"
#include "../../main/includes/events.h"

const char *ONE_WIRE_TAG = "onewire";
const char *ONE_WIRE_TASK_TAG = "onewire-task";

onewire_addr_t addresses[ONEWIRE_MAX_SENSORS] = {0};

um_onewire_sensor_t sensors[ONEWIRE_MAX_SENSORS];

TaskHandle_t onewire_task_handle = NULL;

void onewire_task(void *arg)
{
    char string_address[18] = {0};
    while (true)
    {
        for (int i = 0; i < ONEWIRE_MAX_SENSORS; i++)
        {
            if (addresses[i] != 0)
            {
                // queue_payload_ds18x20_t

                onewire_uint64_t_to_addr_str(addresses[i], string_address);
                uint8_t family_id = (uint8_t)addresses[i];
                // family_id = (uint8_t)addresses[i];
                switch (family_id)
                {
                case DS18X20_FAMILY_DS18B20:  // CASE ONE-WIRE TEMP SENSOR
                case DS18X20_FAMILY_DS18S20:  // CASE ONE-WIRE TEMP SENSOR
                case DS18X20_FAMILY_DS1822:   // CASE ONE-WIRE TEMP SENSOR
                case DS18X20_FAMILY_MAX31850: // CASE ONE-WIRE TEMP SENSOR
                    float temp;
                    esp_err_t res = ds18x20_read_temp(addresses[i], &temp);

                    if (res == ESP_OK)
                    {
                        ESP_LOGI(ONE_WIRE_TASK_TAG, "[%s]: temp is: %.2f°C, family_id: %d", string_address, temp, family_id);
                        um_ev_message_onewire message = {
                            .sn = string_address,
                            .temp = temp};
                        esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_OW, &message, sizeof(message), portMAX_DELAY);

                        um_onewire_update_state(addresses[i], temp);
                        //  free(buff);
                    }
                    break;

                default:
                    break;
                }
            }
        }
        vTaskDelay(ONEWIRE_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void um_onewire_update_state(uint64_t address, float temp)
{
    for (int i = 0; i < ONEWIRE_MAX_SENSORS; i++)
    {
        if (sensors[i].address != 0 && sensors[i].address == address)
        {
            sensors[i].value = temp;
            break;
        }
    }
}

void onewire_configure()
{
    ESP_LOGI(ONE_WIRE_TAG, "Starting one-wire bus with ONEWIRE_MAX_SENSORS = %d", ONEWIRE_MAX_SENSORS);

    onewire_reset(ONE_WIRE_PIN);
    onewire_skip_rom(ONE_WIRE_PIN);

    um_onewire_init();
}

um_onewire_sensor_t *onewire_get_sensors()
{
    return sensors;
}

void um_onewire_prepare_config_file()
{
    bool has_content = false;
    int size = ONEWIRE_MAX_SENSORS;

    cJSON *root = cJSON_CreateArray();
    cJSON *el = NULL;

    char *content = um_config_get_config_file(CONFIG_FILE_ONEWIRE);
    if (content != NULL && strlen(content) > 10)
    {
        has_content = true;
    }
    if (has_content)
    {
        root = cJSON_Parse(content);
        if (cJSON_IsInvalid(root))
        {
            has_content = false;
            root = cJSON_CreateArray();
        }
    }

    uint64_t checking_addr;

    // Loop through sensors
    for (int i = 0; i < size; i++)
    {
        if (sensors[i].address == 0)
            continue;

        // Compare sensor with config sensor
        bool founded = false;
        char string_addr[21];
        sprintf(string_addr, "%08llx", sensors[i].address);
        cJSON_ArrayForEach(el, root)
        {
            char *item_add_json = cJSON_GetObjectItem(el, "sn")->valuestring;
            onewire_addr_str_to_uint64_t(item_add_json, &checking_addr);
            if (checking_addr == sensors[i].address)
            {
                founded = true;
                cJSON_SetBoolValue(cJSON_GetObjectItem(el, "active"), founded);
            }
        }
        if (founded)
        {
            ESP_LOGI(ONE_WIRE_TAG, "Sensor %08llx exists in configutation file, skipping", sensors[i].address);
        }
        else
        {
            ESP_LOGI(ONE_WIRE_TAG, "Sensor %08llx not found in configutation file, let`s add it", sensors[i].address);

            cJSON *sensor = cJSON_CreateObject();
            cJSON_AddStringToObject(sensor, "label", string_addr);
            cJSON_AddStringToObject(sensor, "sn", string_addr);
            cJSON_AddBoolToObject(sensor, "active", true);
            cJSON_AddItemToArray(root, sensor);
        }
    }
    el = NULL;
    // Check INACTIVE sensors
    cJSON_ArrayForEach(el, root) // now loop from all json to find innactive sensors
    {
        checking_addr = 0;
        char *el_sn = cJSON_GetObjectItem(el, "sn")->valuestring;
        onewire_addr_str_to_uint64_t(el_sn, &checking_addr);

        bool exists = false;

        for (int j = 0; j < ONEWIRE_MAX_SENSORS; j++)
        {
            if (sensors[j].address == 0)
                continue;
            if (checking_addr == sensors[j].address) // sensor found in all json config
            {
                exists = true;
            }
        }

        if (!exists) // sensor not found, set inactive
        {
            ESP_LOGE(ONE_WIRE_TAG, "Set not founded sensor %08llx inactive (active:false)", checking_addr);
        }
        cJSON *active = cJSON_HasObjectItem(el, "active") ? cJSON_GetObjectItem(el, "active") : NULL;
        if (active != NULL)
        {
            cJSON_SetBoolValue(cJSON_GetObjectItem(el, "active"), exists);
        }
    }

    um_config_write_config_file(CONFIG_FILE_ONEWIRE, root);
    //  char *c = cJSON_PrintUnformatted(root);
    //  ESP_LOGW("!!!!!!!!!", "%s", c);
    cJSON_Delete(root);

    free((void *)content);
}

void um_onewire_init()
{
    // esp_err_t reading_status;
    onewire_search_t onewire_search;
    onewire_addr_t addr;
    int found = 0;

    onewire_search_start(&onewire_search);

    while ((addr = onewire_search_next(&onewire_search, ONE_WIRE_PIN)) != ONEWIRE_NONE)
    {
        uint8_t family_id = (uint8_t)addr;
        if (family_id == DS18X20_FAMILY_DS18S20 || family_id == DS18X20_FAMILY_DS1822 || family_id == DS18X20_FAMILY_DS18B20 || family_id == DS18X20_FAMILY_MAX31850)
        {
            if (found < ONEWIRE_MAX_SENSORS)
            {
                addresses[found] = addr;
            }

            found += 1;
        }
    }
    if (found > 0) // if sensors founded
    {
        for (int i = 0; i < ONEWIRE_MAX_SENSORS; i++) // switch all founded addresses
        {
            if (addresses[i] != 0) // if address != 0, continue
            {
                // https: // stackoverflow.com/questions/8323159/how-to-convert-uint64-t-value-in-const-char-string
                //  length of 2**64 - 1, +1 for nul.
                char string_number[21];
                sensors[i].active = true;
                sensors[i].address = (uint64_t)addresses[i];

                onewire_uint64_t_to_addr_str(sensors[i].address, string_number);
                ESP_LOGI(ONE_WIRE_TAG, "Found sensor %s", string_number);
            }
        }
    }
    else
    {
        ESP_LOGW(ONE_WIRE_TAG, "No 1-wire sensors found");
    }

    esp_event_post(APP_EVENTS, EV_ONEWIRE_INIT, NULL, sizeof(NULL), portMAX_DELAY);

    xTaskCreatePinnedToCore(onewire_task, "onewire_task", 4096, NULL, ONE_WIRE_TASK_PRIORITY, &onewire_task_handle, 1);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    um_onewire_prepare_config_file();
}

void onewire_addr_str_to_uint64_t(char *address, uint64_t *out)
{
    *out = (uint64_t)strtoull(address, NULL, 16); // aka 0xf0fbbb791f64ff28
}

void onewire_uint64_t_to_addr_str(onewire_addr_t address, char *out)
{
    sprintf(out, "%08llx", (uint64_t)address);
}

esp_err_t ds18x20_read_temp(onewire_addr_t address, float *temp)
{
    return ds18x20_measure_and_read(ONE_WIRE_PIN, address, temp);
}
