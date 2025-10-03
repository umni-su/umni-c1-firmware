#include "sd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#include "../../main/includes/events.h"

// Время подавления дребезга контактов в миллисекундах
#define DEBOUNCE_DELAY_MS 50

static const char *TAG = "SD";
static TickType_t last_interrupt_time = 0;
static sdmmc_card_t *sd_card = NULL;

/**
 * @brief Задача обработки прерывания детектора SD карты
 */
static void um_sd_cd_interrupt_task(void *arg)
{
    int level = gpio_get_level(CONFIG_UMNI_SD_CD);
    printf("Level %d ", level);
    if (level == 0)
    {
        esp_event_post(APP_EVENTS, EV_SDCARD_PUSH_IN, NULL, sizeof(NULL), portMAX_DELAY);
        ESP_LOGI(TAG, "SD card was inserted %d", level);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        um_sd_mount();
    }
    else
    {
        esp_event_post(APP_EVENTS, EV_SDCARD_PUSH_OUT, NULL, sizeof(NULL), portMAX_DELAY);
        ESP_LOGW(TAG, "SD card was ejected %d", level);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        um_sd_unmount();
    }

    vTaskDelete(NULL);
}

/**
 * @brief Обработчик прерывания GPIO в режиме IRAM
 */
static void IRAM_ATTR um_catch_sd_cd_interrupts(void *args)
{
    TickType_t current_time = xTaskGetTickCountFromISR();

    // Подавление дребезга - проверяем время с последнего прерывания
    if ((current_time - last_interrupt_time) * portTICK_PERIOD_MS >= DEBOUNCE_DELAY_MS)
    {
        xTaskCreate(um_sd_cd_interrupt_task, "sd_cd_interrupt_task", 4096, NULL, 2, NULL);
    }

    last_interrupt_time = current_time;
}

/**
 * @brief Инициализация детектора SD карты
 */
void um_init_sd_cd(void)
{
    // Убедитесь, что служба прерываний GPIO установлена
    gpio_install_isr_service(0);

    gpio_set_direction(CONFIG_UMNI_SD_CD, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_UMNI_SD_CD, GPIO_FLOATING);
    gpio_isr_handler_add(CONFIG_UMNI_SD_CD, um_catch_sd_cd_interrupts, NULL);
    gpio_set_intr_type(CONFIG_UMNI_SD_CD, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(CONFIG_UMNI_SD_CD);

    ESP_LOGI(TAG, "SD CD interrupt handler initialized with debouncing");
}

/**
 * @brief Проверяет текущее состояние детектора SD карты
 */
bool um_sd_card_detected(void)
{
    int level = gpio_get_level(CONFIG_UMNI_SD_CD);
    return (level == 0);
}

/**
 * @brief Инициализирует начальное сотояние SD карты
 */
esp_err_t um_sdcard_init()
{
    // Инициализация детектора
    um_init_sd_cd();

    // Проверка наличия карты
    if (um_sd_card_detected())
    {
        // Монтирование карты
        return um_sd_mount();
    }
    return ESP_FAIL;
}

/**
 * @brief Монтирует SD карту в файловую систему
 */
esp_err_t um_sd_mount(void)
{
    esp_err_t ret;

    // Конфигурация монтирования FAT
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // Конфигурация устройства SD SPI
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_UMNI_SD_CS;

    // Настройка хоста SD SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 12 * 1000; // Пониженная частота для стабильности
    slot_config.host_id = CONFIG_UMNI_ETH_SPI_HOST;

    // Монтирование SD карты
    ret = esp_vfs_fat_sdspi_mount(CONFIG_UMNI_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card mounted successfully");
        // Вывод информации о карте
        sdmmc_card_print_info(stdout, sd_card);
        esp_event_post(APP_EVENTS, EV_SDCARD_MOUNTED, NULL, sizeof(NULL), portMAX_DELAY);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        esp_event_post(APP_EVENTS, EV_SDCARD_UNMOUNTED, NULL, sizeof(NULL), portMAX_DELAY);
    }

    return ret;
}

/**
 * @brief Размонтирует SD карту
 */
esp_err_t um_sd_unmount(void)
{
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(CONFIG_UMNI_SD_MOUNT_POINT, sd_card);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card unmounted successfully");
        sd_card = NULL;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Получает информацию о SD карте
 */
void *um_sd_get_card_info(void)
{
    return sd_card;
}