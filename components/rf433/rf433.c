#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "rf433.h"
#include "esp32_rf_receiver.h"

#include "../config/config.h"

static const char *TAG = "rf433";

static um_rf_devices_t rf_devices[MAX_SENSORS];

static um_rf_devices_t rf_scanned_devices[MAX_SEARCH_SENSORS];

static QueueHandle_t esp_rf433_queue = NULL;

static bool search = false;

void um_rf433_receiver_task(void *pvParameter)
{
    uint8_t prot_num = 0;
    esp_rf433_queue = (QueueHandle_t)pvParameter;
    while (1)
    {
        if (esp_rf433_queue != NULL && xQueueReceive(esp_rf433_queue, &prot_num, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(TAG, "RF433 interrurpt fail");
        }
        else
        {
            uint32_t all = esp_rf433_get_received_value();
            int chan4 = all >> 0 & 0x01;
            int chan3 = all >> 1 & 0x01;
            int chan2 = all >> 2 & 0x01;
            int chan1 = all >> 3 & 0x01;

            uint8_t state = (uint8_t)all;

            // uint32_t number = data && 0XFF;
            uint32_t number = all >> 4;

            int existing_index = um_rf433_get_existing_index(rf_devices, number, MAX_SENSORS);

            bool show = true;

            um_rf_devices_t dev;

            float div = 0;

            // if (existing_index == -1)
            // {
            //     dev.serial = number;
            //     dev.time = esp_timer_get_time();
            //     rf_devices[um_rf433_get_array_length(rf_devices, MAX_SENSORS)] = dev;
            // }
            // else
            if (existing_index > -1)
            {
                dev = rf_devices[existing_index];
                // Смотрим состояние датчика. Если срабатывания не было, то устанавливаем флаг срабатывания
                rf_devices[existing_index].time = esp_timer_get_time();
                div = (rf_devices[existing_index].time - dev.time) / 1000;

                dev.triggered = !dev.triggered && div > 200;
                // ESP_LOGI(TAG, "%.1f : Existing serial number %06lX, time %ld", div, dev.serial, rf_devices[existing_index].time);
            }

            dev.state = state;

            // show = (dev.triggered) || existing_index == -1;
            show = dev.triggered;
            if (show)
            {
                ESP_LOGW(TAG, "Received %lu / %dbit Protocol: %d", all, esp_rf433_get_received_bit_length(), prot_num);
                ESP_LOGI(TAG, "Serial number %06lX, time %ld, index %d", dev.serial, dev.time, existing_index);
                ESP_LOGI(TAG, "A Chan 1: %d, ", chan1);
                ESP_LOGI(TAG, "B Chan 2: %d, ", chan2);
                ESP_LOGI(TAG, "C Chan 3: %d, ", chan3);
                ESP_LOGI(TAG, "D Chan 4: %d \n", chan4);

                int ind = um_rf433_get_existing_index(rf_devices, number, MAX_SENSORS);

                if (rf_devices[ind].triggered)
                {
                    // rf_devices[ind].serial = 0;
                    rf_devices[ind].time = 0;
                    rf_devices[ind].triggered = false;
                    ESP_LOGI(TAG, "Delete indexes");
                }
            }

            if (search)
            {
                um_rf_devices_t search_dev = {
                    .serial = number,
                    .state = state};
                // search is mode active
                int search_array_length = um_rf433_get_array_length(rf_scanned_devices, MAX_SEARCH_SENSORS);
                // int existing = um_rf_
                if (search_array_length < MAX_SEARCH_SENSORS)
                {
                    int existing_search_index = um_rf433_get_existing_index(rf_scanned_devices, search_dev.serial, MAX_SEARCH_SENSORS);
                    if (existing_search_index == -1)
                    {
                        rf_scanned_devices[search_array_length] = search_dev;
                    }
                    else
                    {
                        rf_scanned_devices[existing_search_index] = search_dev;
                    }
                }
            }
            else
            {
                um_rf433_clear_search();
            }

            esp_rf433_reset_available();
        }
    }
    vTaskDelete(NULL);
}

short int um_rf433_get_existing_index(um_rf_devices_t *devices, uint32_t number, int max)
{
    for (size_t i = 0; i < max; i++)
    {
        if (devices[i].serial == number)
        {
            return i;
        }
    }
    return -1;
}

short int um_rf433_get_array_length(um_rf_devices_t *devices, int max)
{
    short int count = 0;
    for (size_t i = 0; i < max; i++)
    {
        if (devices[i].serial > 0)
        {
            count++;
        }
    }
    return count;
}

void um_rf_433_init()
{
    um_rf433_get_config_file();
    um_rf433_add_sensors_from_config();
    esp_rf433_initialize(CONFIG_UMNI_RF433_REC_PIN, &um_rf433_receiver_task);
}

void um_rf433_add_sensors_from_config()
{
    // обнуляем массив сенсоров
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        rf_devices[i].alarm = false;
        rf_devices[i].serial = 0;
        rf_devices[i].state = 0;
        rf_devices[i].time = 0;
        rf_devices[i].triggered = false;
    }

    char *config = um_config_get_config_file(CONFIG_FILE_RF433);
    cJSON *array = cJSON_Parse(config);
    cJSON *el = NULL;
    int index = 0;
    cJSON_ArrayForEach(el, array)
    {
        bool has_serial = cJSON_HasObjectItem(el, "serial") && cJSON_GetObjectItem(el, "serial") != NULL;
        if (has_serial)
        {
            rf_devices[index].serial = cJSON_GetObjectItem(el, "serial")->valueint;
            rf_devices[index].alarm = cJSON_IsTrue(cJSON_GetObjectItem(el, "alarm"));
            rf_devices[index].state = cJSON_HasObjectItem(el, "state") ? cJSON_GetObjectItem(el, "state")->valueint : 0;
        }
        index++;
    }
}

void um_rf433_get_config_file()
{
    char *contents = um_config_get_config_file(CONFIG_FILE_RF433);

    cJSON *config = NULL;

    if (contents == NULL)
    {
        config = cJSON_CreateArray();
        if (um_config_write_config_file(CONFIG_FILE_RF433, config))
        {
            contents = cJSON_PrintUnformatted(config);
            ESP_LOGI(TAG, "RF Config file created successfully: %s", contents);
        }
    }
    else
    {
        ESP_LOGI(TAG, "RF Config file is: %s", contents);
    }
    free(contents);
    cJSON_Delete(config);
}

/**
 * Simple start-stop timer
 */
void um_rf433_search_handle(void *arg)
{
    // алгоритм поиска
    // установка режима поиска

    // создание задачи на определенный интервал времени поиска
    // возможно в процессе проверять есть датчик в конфигурации или нет
    // запись в структуру сенсоров поиска срабатываемые датчики
    // по окончанию задачи сбрасывать режим поиска
    search = true;
    vTaskDelay(pdMS_TO_TICKS(SEARCH_TIMEOUT));

    um_rf433_clear_search();

    search = false;

    vTaskDelete(NULL);
}

void um_rf433_clear_search()
{
    for (int i = 0; i < MAX_SEARCH_SENSORS; i++)
    {
        rf_scanned_devices[i].alarm = false;
        rf_scanned_devices[i].serial = 0;
        rf_scanned_devices[i].time = 0;
        rf_scanned_devices[i].triggered = false;
        rf_scanned_devices[i].state = 0;
    }
}

um_rf_devices_t *um_rf433_get_search_result()
{
    return rf_scanned_devices;
}

void um_rf433_activale_search()
{
    if (search)
        return;
    xTaskCreatePinnedToCore(um_rf433_search_handle, "rf433_search", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL, 1);
}
