#include <inttypes.h>
#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <pcf8574.h>
#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"

#include "../components/ethernet/ethernet.h"
#include "../components/webserver/webserver.h"
#include "../components/nvs/nvs.h"
#include "../components/adc/adc.h"
#include "../components/1wire/1wire.h"
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static i2c_dev_t pcf8574;

static i2c_dev_t pcf8574_inp;

static TaskHandle_t do_handle;
static TaskHandle_t di_handle;

#define I2C_ADDR 0x27
#define I2C_MASTER_SCL 22
#define I2C_MASTER_SDA 21
#define INP_I2C_ADDR 0x26
#define INT_PIN GPIO_NUM_16

static void IRAM_ATTR catch_interrupts(void *args)
{
    ESP_DRAM_LOGI("IRAM", "Catch!");
}

void test(void *pvParameters)
{
    uint8_t data = 0xff;
    memset(&pcf8574, 0, sizeof(i2c_dev_t));
    pcf8574.cfg.master.clk_speed = 5000; // Hz
    ESP_ERROR_CHECK(pcf8574_init_desc(&pcf8574, I2C_ADDR, 0, I2C_MASTER_SDA, I2C_MASTER_SCL));
    pcf8574_port_write(&pcf8574, data);
    uint8_t res = 0xff;
    while (true)
    {
        ESP_LOGI(TAG, "=========");

        // data = ~data;

        pcf8574_port_read(&pcf8574, &res);
        for (int i = 0; i < 8; i++)
        {
            int level = res >> i & 0x01;
            if (level != 1)
            {
                res = res | (1 << i);
            }
            else
            {
                res = res & ~(1 << i);
            }
            pcf8574_port_write(&pcf8574, res);
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Read data from pcf %d - %d", i, level);
        }
        size_t total_heap_size = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        ESP_LOGW("HEAP", "\r\nHEAP total: %d, free: %d\r\n", total_heap_size, free_heap_size);
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void test_inp(void *pvParameters)
{
    memset(&pcf8574_inp, 0, sizeof(i2c_dev_t));
    pcf8574_inp.cfg.master.clk_speed = 5000; // Hz
    ESP_ERROR_CHECK(pcf8574_init_desc(&pcf8574_inp, INP_I2C_ADDR, 0, I2C_MASTER_SDA, I2C_MASTER_SCL));
    uint8_t inp = 0xff;
    pcf8574_port_write(&pcf8574_inp, inp);
    vTaskDelete(NULL);
}

void watch_any_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGI(TAG, "EVENT IS %08lX", id);
}

#if CONFIG_EXAMPLE_WEB_DEPLOY_SEMIHOST
esp_err_t init_fs(void)
{
    esp_err_t ret = esp_vfs_semihost_register(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WEBSERVER_TAG, "Failed to register semihost driver (%s)!", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif

#if CONFIG_EXAMPLE_WEB_DEPLOY_SD

esp_err_t init_fs(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.gpio_cs = GPIO_NUM_17;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    slot_config.host_id = CONFIG_EXAMPLE_ETH_SPI_HOST;
    // ret = spi_bus_add_device(CONFIG_EXAMPLE_ETH_SPI_HOST,  &buscfg, NULL);
    ESP_ERROR_CHECK(esp_vfs_fat_sdspi_mount(CONFIG_EXAMPLE_WEB_MOUNT_POINT, &host, &slot_config, &mount_config, &card));
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

#endif

void app_main(void)
{

    ESP_LOGI(TAG, "!Starting load UMNI firmware!");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &watch_any_event, NULL));

    ESP_ERROR_CHECK(init_nvs());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    gpio_set_direction(INT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INT_PIN, GPIO_FLOATING);
    gpio_isr_handler_add(INT_PIN, catch_interrupts, NULL);
    gpio_set_intr_type(INT_PIN, GPIO_INTR_NEGEDGE);
    // gpio_intr_enable(INT_PIN);

    // TEST DIO
    // xTaskCreatePinnedToCore(test, "do", 4095, NULL, 13, &do_handle, 1);
    // xTaskCreatePinnedToCore(test_inp, "di", 4095, NULL, 13, &di_handle, 1);
    // TEST DIO END

    vTaskDelay(pdMS_TO_TICKS(200));
    ethernet_start();
    // webserver_start();
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.gpio_cs = CONFIG_UMNI_SD_CS;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    slot_config.host_id = 1;
    // ret = spi_bus_add_device(CONFIG_EXAMPLE_ETH_SPI_HOST,  &buscfg, NULL);
    ESP_ERROR_CHECK(esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card));
    sdmmc_card_print_info(stdout, card);

    init_adc();

    onewire_init_config();
}
