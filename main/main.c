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
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

static const char *TAG = "main";

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static i2c_dev_t pcf8574_inp;

static TaskHandle_t do_handle;
static TaskHandle_t di_handle;

void watch_any_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGI(TAG, "EVENT IS %08lX, %d", id, (int)id);
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

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(init_nvs());

    ESP_ERROR_CHECK(init_do());
    ESP_ERROR_CHECK(init_di());

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

    do_set_level(DO_1, DO_HIGH);
    do_set_level(DO_2, DO_LOW);
    do_set_level(DO_3, DO_HIGH);
    do_set_level(DO_4, DO_LOW);
    do_set_level(DO_5, DO_HIGH);
    do_set_level(DO_6, DO_LOW);

    // do_set_level(LED_STAT, DO_HIGH);
    // do_set_level(LED_ERR, DO_HIGH);

    for (int i = 0; i < 6; i++)
    {
        ESP_LOGI("do", "DO state %d: %d", i, do_get_level(i));
    }
}
