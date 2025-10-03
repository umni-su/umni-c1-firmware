#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#include "rf433.h"
#include "esp32_rf_receiver.h"
#include "esp_event.h"

#include "../config/config.h"
#include "../automation/automation.h"
#include "../../main/includes/events.h"

static const char *TAG = "rf433";

static um_rf_devices_t rf_devices[MAX_SENSORS];
static um_rf_devices_t rf_scanned_devices[MAX_SEARCH_SENSORS];
QueueHandle_t esp_rf433_queue = NULL;
static um_am_main_t rf_automations[MAX_SENSORS];
static bool search = false;

// Глобальные переменные для калибровки
static rf433_calibration_t rf433_calibration = {
    .total_packets = 0,
    .processed_packets = 0,
    .duplicate_packets = 0,
    .error_packets = 0,
    .receive_tolerance = 60,
    .separation_limit = 4300,
    .debounce_time_ms = RF433_DEBOUNCE_TIME_MS,
    .min_packets = RF433_MIN_PACKETS};

// Обновление статистики
static void update_rf433_stats(bool processed, bool duplicate, bool error)
{
    rf433_calibration.total_packets++;
    if (processed)
        rf433_calibration.processed_packets++;
    if (duplicate)
        rf433_calibration.duplicate_packets++;
    if (error)
        rf433_calibration.error_packets++;
}

// Обработка alarm-пакетов (высший приоритет)
static void process_alarm_packet(uint32_t all, int index, uint8_t state)
{
    long current_time = esp_timer_get_time();
    long time_since_last = (current_time - rf_devices[index].last_processed_time) / 1000;

    // УВЕЛИЧИМ окно для alarm-пакетов до 100ms
    if (time_since_last > 100)
    {
        rf_devices[index].triggered = true;
        rf_devices[index].last_processed_time = current_time;
        rf_devices[index].state = state;
        rf_devices[index].packet_count = 0;

        ESP_LOGW(TAG, "ALARM! Sensor: %06lX, State: %d", rf_devices[index].serial, state);

        um_ev_message_rf433 message = {
            .alarm = rf_devices[index].alarm,
            .serial = rf_devices[index].serial,
            .state = rf_devices[index].state,
            .triggered = rf_devices[index].triggered};

        esp_event_post(APP_EVENTS, EV_RF433_SENSOR, &message, sizeof(message), portMAX_DELAY);

        if (rf_automations[index].ext)
        {
            rf_automations[index].value = state;
            um_am_automation_run(&rf_automations[index]);
        }

        update_rf433_stats(true, false, false);
    }
    else
    {
        update_rf433_stats(false, true, false);
    }
}

// Обработка обычных пакетов
static void process_normal_packet(uint32_t all, int index, uint8_t state)
{
    um_rf_devices_t *device = &rf_devices[index];
    long current_time = esp_timer_get_time();
    long time_since_last = (current_time - device->last_processed_time) / 1000;

    // УПРОЩЕННАЯ ЛОГИКА - обрабатываем почти все пакеты с минимальным дебаунсингом
    if (time_since_last > 200) // УВЕЛИЧИМ до 200ms для обычных пакетов
    {
        device->triggered = true;
        device->last_processed_time = current_time;
        device->state = state;
        device->packet_count = 0;

        ESP_LOGI(TAG, "Normal packet - Serial: %06lX, State: %d", device->serial, state);

        um_ev_message_rf433 message = {
            .alarm = device->alarm,
            .serial = device->serial,
            .state = device->state,
            .triggered = device->triggered};

        esp_event_post(APP_EVENTS, EV_RF433_SENSOR, &message, sizeof(message), portMAX_DELAY);

        if (rf_automations[index].ext)
        {
            rf_automations[index].value = state;
            um_am_automation_run(&rf_automations[index]);
        }

        update_rf433_stats(true, false, false);
    }
    else
    {
        // Даже если не обрабатываем, обновляем состояние
        device->state = state;
        update_rf433_stats(false, true, false);
    }
}

void um_rf433_receiver_task(void *pvParameter)
{
    uint8_t prot_num = 0;
    static uint32_t last_all = 0;
    static int same_packet_count = 0;
    esp_rf433_queue = (QueueHandle_t)pvParameter;

    while (1)
    {
        if (xQueueReceive(esp_rf433_queue, &prot_num, portMAX_DELAY) == pdTRUE)
        {
            uint32_t all = esp_rf433_get_received_value();

            // ОСЛАБИМ проверку идентичности - пропускаем только подряд идущие одинаковые пакеты
            if (all == last_all)
            {
                same_packet_count++;
                if (same_packet_count > 3) // Пропускаем только после 3+ одинаковых пакетов подряд
                {
                    update_rf433_stats(false, true, false);
                    esp_rf433_reset_available();
                    continue;
                }
            }
            else
            {
                same_packet_count = 0;
            }
            last_all = all;

            int chan4 = all >> 0 & 0x01;
            int chan3 = all >> 1 & 0x01;
            int chan2 = all >> 2 & 0x01;
            int chan1 = all >> 3 & 0x01;

            uint8_t state = 0x00;
            state |= (chan1 << 0) | (chan2 << 1) | (chan3 << 2) | (chan4 << 3);
            uint32_t number = all >> 4;

            int existing_index = um_rf433_get_existing_index(rf_devices, number, MAX_SENSORS);

            if (existing_index > -1)
            {
                // Определяем тип пакета (alarm или обычный)
                if (rf_devices[existing_index].alarm)
                {
                    process_alarm_packet(all, existing_index, state);
                }
                else
                {
                    process_normal_packet(all, existing_index, state);
                }
            }
            else
            {
                // ВСЕГДА обрабатываем новые устройства
                ESP_LOGW(TAG, "New device - Serial: %06lX, State: %d", number, state);

                // Добавляем новое устройство в массив
                int free_index = um_rf433_get_array_length(rf_devices, MAX_SENSORS);
                if (free_index < MAX_SENSORS)
                {
                    rf_devices[free_index].serial = number;
                    rf_devices[free_index].state = state;
                    rf_devices[free_index].time = esp_timer_get_time();
                    rf_devices[free_index].last_processed_time = esp_timer_get_time();
                    rf_devices[free_index].triggered = true;
                    rf_devices[free_index].alarm = false; // По умолчанию не alarm
                    rf_devices[free_index].packet_count = 0;

                    // Отправляем событие о новом устройстве
                    um_ev_message_rf433 message = {
                        .alarm = false,
                        .serial = number,
                        .state = state,
                        .triggered = true};

                    esp_event_post(APP_EVENTS, EV_RF433_SENSOR, &message, sizeof(message), portMAX_DELAY);
                }

                update_rf433_stats(true, false, false);
            }

            // Обработка режима поиска (без изменений)
            if (search)
            {
                um_rf_devices_t search_dev = {
                    .serial = number,
                    .state = state,
                    .time = esp_timer_get_time()};

                int search_array_length = um_rf433_get_array_length(rf_scanned_devices, MAX_SEARCH_SENSORS);
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

            esp_rf433_reset_available();

// ВРЕМЕННО ОТКЛЮЧИМ АВТОКАЛИБРОВКУ для тестирования
#if RF433_AUTO_CALIBRATION && 0
            if (rf433_calibration.total_packets % 50 == 0)
            {
                rf433_auto_calibrate();
            }
#endif
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

    // Применяем калибровочные параметры к приемнику
    esp_rf433_set_receive_tolerance(rf433_calibration.receive_tolerance);
    esp_rf433_set_separation_limit(rf433_calibration.separation_limit);
}

um_rf_devices_t um_rf433_get_sensor(int serial)
{
    int count = um_rf433_get_array_length(rf_devices, MAX_SENSORS);
    um_rf_devices_t device = {0};
    for (int i = 0; i < count; i++)
    {
        if (rf_devices[i].serial == serial)
        {
            device = rf_devices[i];
            break;
        }
    }
    return device;
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
        rf_devices[i].last_processed_time = 0;
        rf_devices[i].packet_count = 0;
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

            um_am_parse_json_config(el, &rf_automations[index]);
        }
        index++;
    }
    free((void *)config);
    cJSON_Delete(array);
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

void um_rf433_search_handle(void *arg)
{
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
        rf_scanned_devices[i].last_processed_time = 0;
        rf_scanned_devices[i].packet_count = 0;
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
    xTaskCreate(um_rf433_search_handle, "rf433_search", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL);
}

// Функции калибровки
#if RF433_AUTO_CALIBRATION
void rf433_auto_calibrate()
{
    if (rf433_calibration.total_packets < 20)
        return; // Недостаточно данных

    float success_rate = (float)rf433_calibration.processed_packets / rf433_calibration.total_packets * 100;
    float duplicate_rate = (float)rf433_calibration.duplicate_packets / rf433_calibration.total_packets * 100;

    ESP_LOGI(TAG, "Calibration: Success=%.1f%%, Duplicates=%.1f%%, Total=%lu",
             success_rate, duplicate_rate, rf433_calibration.total_packets);

    // Автоматическая регулировка параметров
    if (duplicate_rate > 30.0)
    {
        // Слишком много дубликатов - увеличиваем дебаунсинг
        rf433_calibration.debounce_time_ms = min(rf433_calibration.debounce_time_ms + 100, 2000);
        rf433_calibration.min_packets = min(rf433_calibration.min_packets + 1, 5);
        ESP_LOGW(TAG, "Auto-calibration: increased debounce to %dms, min_packets to %d",
                 rf433_calibration.debounce_time_ms, rf433_calibration.min_packets);
    }
    else if (duplicate_rate < 10.0 && rf433_calibration.debounce_time_ms > RF433_DEBOUNCE_TIME_MS)
    {
        // Мало дубликатов - уменьшаем дебаунсинг (но не ниже базового значения)
        rf433_calibration.debounce_time_ms = max(rf433_calibration.debounce_time_ms - 50, RF433_DEBOUNCE_TIME_MS);
        rf433_calibration.min_packets = max(rf433_calibration.min_packets - 1, RF433_MIN_PACKETS);
    }

    if (success_rate < 70.0)
    {
        // Низкий процент успешного приема - увеличиваем допуск
        rf433_calibration.receive_tolerance = min(rf433_calibration.receive_tolerance + 5, 80);
        esp_rf433_set_receive_tolerance(rf433_calibration.receive_tolerance);
        ESP_LOGW(TAG, "Auto-calibration: increased tolerance to %d%%", rf433_calibration.receive_tolerance);
    }
    else if (success_rate > 90.0 && rf433_calibration.receive_tolerance > 60)
    {
        // Высокий процент успеха - уменьшаем допуск для лучшей фильтрации
        rf433_calibration.receive_tolerance = max(rf433_calibration.receive_tolerance - 2, 40);
        esp_rf433_set_receive_tolerance(rf433_calibration.receive_tolerance);
    }
}
#else
void rf433_auto_calibrate()
{
    // Функция пустая если калибровка отключена
}
#endif

rf433_calibration_t *um_rf433_get_calibration_data()
{
    return &rf433_calibration;
}

void um_rf433_reset_calibration()
{
    rf433_calibration.total_packets = 0;
    rf433_calibration.processed_packets = 0;
    rf433_calibration.duplicate_packets = 0;
    rf433_calibration.error_packets = 0;
    rf433_calibration.receive_tolerance = 60;
    rf433_calibration.separation_limit = 4300;
    rf433_calibration.debounce_time_ms = RF433_DEBOUNCE_TIME_MS;
    rf433_calibration.min_packets = RF433_MIN_PACKETS;

    // Применяем сброшенные параметры к приемнику
    esp_rf433_set_receive_tolerance(rf433_calibration.receive_tolerance);
    esp_rf433_set_separation_limit(rf433_calibration.separation_limit);
}