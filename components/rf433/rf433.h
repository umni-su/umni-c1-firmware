#include <stdint.h>

#define MAX_SENSORS 32
#define MAX_SEARCH_SENSORS 5
#define SEARCH_TIMEOUT 15000

// Конфигурация калибровки
#define RF433_AUTO_CALIBRATION 0     // ВРЕМЕННО ВЫКЛЮЧИТЬ для отладки
#define RF433_DEBOUNCE_TIME_MS 200   // УВЕЛИЧИТЬ базовое значение
#define RF433_MIN_PACKETS 1          // УМЕНЬШИТЬ до 1
#define RF433_RECEIVER_DEBOUNCE_MS 0 // ВЫКЛЮЧИТЬ дебаунсинг приемника

typedef struct
{
    uint32_t serial;
    long time;
    long last_processed_time; // Время последней обработки
    bool alarm;
    bool triggered;
    uint8_t state;
    uint8_t packet_count; // Счетчик пакетов для дебаунсинга
} um_rf_devices_t;

// Структура для статистики и калибровки
typedef struct
{
    uint32_t total_packets;
    uint32_t processed_packets;
    uint32_t duplicate_packets;
    uint32_t error_packets;
    int receive_tolerance;
    unsigned separation_limit;
    unsigned debounce_time_ms;
    unsigned min_packets;
} rf433_calibration_t;

void um_rf_433_init();
void um_rf433_receiver_task(void *pvParameter);
short int um_rf433_get_existing_index(um_rf_devices_t *devices, uint32_t number, int max);
short int um_rf433_get_array_length(um_rf_devices_t *devices, int max);
void um_rf433_get_config_file();
void um_rf433_activale_search();
void um_rf433_clear_search();
um_rf_devices_t *um_rf433_get_search_result();
void um_rf433_add_sensors_from_config();
um_rf_devices_t um_rf433_get_sensor(int serial);

// Функции калибровки
void rf433_auto_calibrate();
rf433_calibration_t *um_rf433_get_calibration_data();
void um_rf433_reset_calibration();