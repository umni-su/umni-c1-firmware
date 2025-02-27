#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_crt_bundle.h"

#include "../../main/includes/events.h"

#define CONFIG_OTA_ATTEMPTS 5

// EXAMPLE
#define URL "https://share.umni.su/f/df57ee6136b548ec86c5/?dl=1"

#include "ota.h"

const char *OTA_TAG = "ota";

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT)
    {
        switch (event_id)
        {
        case ESP_HTTPS_OTA_START:
            ESP_LOGI(OTA_TAG, "OTA started");
            esp_event_post(APP_EVENTS, EV_OTA_START, NULL, sizeof(NULL), portMAX_DELAY);
            break;
        case ESP_HTTPS_OTA_CONNECTED:
            ESP_LOGI(OTA_TAG, "Connected to server");
            break;
        case ESP_HTTPS_OTA_GET_IMG_DESC:
            ESP_LOGI(OTA_TAG, "Reading Image Description");
            break;
        case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
            ESP_LOGI(OTA_TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
            break;
        case ESP_HTTPS_OTA_DECRYPT_CB:
            ESP_LOGI(OTA_TAG, "Callback to decrypt function");
            break;
        case ESP_HTTPS_OTA_WRITE_FLASH:
            ESP_LOGD(OTA_TAG, "Writing to flash: %d written", *(int *)event_data);
            break;
        case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
            ESP_LOGI(OTA_TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
            break;
        case ESP_HTTPS_OTA_FINISH:
            ESP_LOGI(OTA_TAG, "OTA finish");
            esp_event_post(APP_EVENTS, EV_OTA_SUCCESS, NULL, sizeof(NULL), portMAX_DELAY);
            break;
        case ESP_HTTPS_OTA_ABORT:
            ESP_LOGI(OTA_TAG, "OTA abort");
            esp_event_post(APP_EVENTS, EV_OTA_ABORT, NULL, sizeof(NULL), portMAX_DELAY);
            break;
        }
    }
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(OTA_TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0)
    {
        ESP_LOGW(OTA_TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif
    return ESP_OK;
}

void um_ota_start_upgrade_task()
{
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // Настраиваем HTTPS
    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        //.timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        //.http_client_init_cb = _http_client_init_cb,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(OTA_TAG, "ESP HTTPS OTA Begin failed");
        esp_event_post(APP_EVENTS, EV_OTA_ABORT, NULL, sizeof(NULL), portMAX_DELAY);
        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(OTA_TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(OTA_TAG, "image header verification failed");
        goto ota_end;
    }

    while (1)
    {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(OTA_TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
    {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(OTA_TAG, "Complete data was not received.");
    }
    else
    {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK))
        {
            ESP_LOGI(OTA_TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(OTA_TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(OTA_TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(OTA_TAG, "ESP_HTTPS_OTA upgrade failed");
    vTaskDelete(NULL);
}

void um_ota_init()
{
    xTaskCreate(&um_ota_start_upgrade_task, "ota", 1024 * 8, NULL, 3, NULL);
}

void um_ota_mark_valid()
{
    esp_ota_mark_app_valid_cancel_rollback();
}