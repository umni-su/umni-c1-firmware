#include "../../main/includes/events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "1wire.h"

// onewire_addr_t addrs[ONEWIRE_MAX_SENSORS];
//  size_t sensor_count = 0;
const char *ONE_WIRE_TAG = "onewire";
const char *ONE_WIRE_TASK_TAG = "onewire-task";

onewire_addr_t addresses[ONEWIRE_MAX_SENSORS] = {0};
um_onewire_sensor_t sensors[ONEWIRE_MAX_SENSORS];

TaskHandle_t onewire_task_handle = NULL;

void onewire_task(void *arg)
{
    char string_address[16] = {0};
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
                        //  free(buff);
                    }
                    break;

                default:
                    break;
                }
            }
        }
        ESP_LOGW(ONE_WIRE_TAG, "[onewire_task] Free memory: %ld bytes", esp_get_free_heap_size());
        vTaskDelay(ONEWIRE_TASK_TIMEOUT / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void onewire_configure()
{
    ESP_LOGI(ONE_WIRE_TAG, "Starting one-wire bus with ONEWIRE_MAX_SENSORS = %d", ONEWIRE_MAX_SENSORS);

    onewire_reset(ONE_WIRE_PIN);
    onewire_skip_rom(ONE_WIRE_PIN);

    onewire_init_config();
}

um_onewire_sensor_t *onewire_get_sensors()
{
    return sensors;
}

void onewire_init_config()
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
