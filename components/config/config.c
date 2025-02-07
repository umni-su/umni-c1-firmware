#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"

#include "../../main/includes/events.h"
#include "../nvs/nvs.h"
#include "../dio/dio.h"
#include "freertos/FreeRTOS.h"
#include "esp_event.h"

#include "config.h"

const char *CONFIG_TAG = "config";

void um_config_create_dir_if_not_exists(char *path)
{
    char *fullpath = malloc(strlen(path) + strlen(CONFIG_UMNI_SD_MOUNT_POINT) + 1);
    sprintf(fullpath, "%s%s", CONFIG_UMNI_SD_MOUNT_POINT, path);
    DIR *dir = opendir(fullpath);
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
        ESP_LOGW(CONFIG_TAG, "Directory %s exists", fullpath);
    }
    else if (ENOENT == errno)
    {
        mkdir(fullpath, 0755);
        ESP_LOGW(CONFIG_TAG, "Create directory %s", fullpath);
    }
    else
    {
        ESP_LOGE(CONFIG_TAG, "Failed to create directory %s", fullpath);
    }
    free(fullpath);
}

esp_err_t um_config_init()
{
    xTaskCreate(um_config_create_config_file_sensors, "um_config_init", configMINIMAL_STACK_SIZE * 6, NULL, 20, NULL);
    return ESP_OK;
}

char *um_config_get_config_file(char *config_name)
{
    size_t filename_len = strlen(config_name) + strlen(CONFIG_PATH) + strlen(CONFIG_UMNI_SD_MOUNT_POINT) + 1;
    char filename[filename_len];
    sprintf(filename, "%s%s%s", CONFIG_UMNI_SD_MOUNT_POINT, CONFIG_PATH, config_name);
    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        ESP_LOGE(CONFIG_TAG, "Failed to open file %s for reading, len %d", filename, filename_len);
        fclose(file);
        return NULL;
    }
    char *text = um_config_get_text_from_file(file);
    // ESP_LOGI(CONFIG_TAG, "Contents of %s is: %s", filename, text);
    //   ESP_LOGW(CONFIG_TAG, "Config filename is %s", filename);

    // free(file_buf);
    fclose(file);
    return text;
}

bool um_config_write_config_file(char *config_name, cJSON *root)
{
    size_t filename_len = strlen(config_name) + strlen(CONFIG_PATH) + strlen(CONFIG_UMNI_SD_MOUNT_POINT) + 1;
    char filename[filename_len];
    char *_content = cJSON_PrintUnformatted(root);
    sprintf(filename, "%s%s%s", CONFIG_UMNI_SD_MOUNT_POINT, CONFIG_PATH, config_name);

    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        ESP_LOGE(CONFIG_TAG, "Failed to open file %s for writing", filename);
        fclose(file);
        return false;
    }
    fprintf(file, _content);
    fclose(file);
    free((void *)_content);
    cJSON_Delete(root);
    return true;
}

bool um_config_delete_config_file(char *config_name)
{
    size_t filename_len = strlen(config_name) + strlen(CONFIG_PATH) + strlen(CONFIG_UMNI_SD_MOUNT_POINT) + 1;
    char filename[filename_len];

    strcpy(filename, CONFIG_UMNI_SD_MOUNT_POINT);
    strcat(filename, CONFIG_PATH);
    strcat(filename, config_name);
    remove(filename);
    return false;
}

void um_config_create_config_file_sensors()
{
    vTaskDelay(500 / portTICK_PERIOD_MS);
    bool has_content = false;
    um_config_create_dir_if_not_exists(CONFIG_PATH);
    um_config_create_dir_if_not_exists(LOG_PATH);
    char *content = um_config_get_config_file(CONFIG_FILE_SENSORS);
    if (content != NULL)
    {
        has_content = true;
        free(content);
    }

    if (!has_content)
    {
        // Create config file
        cJSON *root = cJSON_CreateObject();
        cJSON *inputs = cJSON_CreateArray();
        cJSON *outputs = cJSON_CreateArray();

        const int do_channels[] = {
            CONFIG_UMNI_DI_CHAN_1,
            CONFIG_UMNI_DI_CHAN_2,
            CONFIG_UMNI_DI_CHAN_3,
            CONFIG_UMNI_DI_CHAN_4,
            CONFIG_UMNI_DI_CHAN_5,
            CONFIG_UMNI_DI_CHAN_6};

        for (int i = 0; i < 6; i++)
        {
            char label_do[10];
            char label_di[10];
            sprintf(label_do, "Relay %d", do_channels[i]);
            cJSON *state_do = cJSON_CreateObject();
            cJSON_AddStringToObject(state_do, "label", label_do);
            cJSON_AddNumberToObject(state_do, "index", i);
            cJSON_AddItemToArray(outputs, state_do);

            sprintf(label_di, "Input %d", do_channels[i]);
            cJSON *state_di = cJSON_CreateObject();
            cJSON_AddStringToObject(state_di, "label", label_di);
            cJSON_AddNumberToObject(state_di, "index", i);
            cJSON_AddItemToArray(inputs, state_di);
        }

        cJSON_AddItemToObject(root, "do", outputs);
        cJSON_AddItemToObject(root, "di", inputs);

        um_config_write_config_file(CONFIG_FILE_SENSORS, root);
    }

    esp_event_post(APP_EVENTS, EV_CONFIGURATION_READY, NULL, sizeof(NULL), portMAX_DELAY);
    vTaskDelete(NULL);
}

// https://ru.stackoverflow.com/questions/833081/file-get-contents-в-c
long um_config_get_file_size(FILE *file)
{
    /* Перемещаемся в конец файла. */
    fseek(file, 0, SEEK_END);

    /* Получаем текущую позицию в файле (размер). */
    const long fsize = ftell(file);
    if (fsize == -1)
        perror("Can't get the current value of the file position.\n");

    /* Перемещаемся в начало файла. */
    rewind(file);
    return fsize;
}
// https://ru.stackoverflow.com/questions/833081/file-get-contents-в-c
char *um_config_get_text_from_file(FILE *file)
{
    /* Получаем размер файла и проверяем, успешно ли получение. */
    const long fsize = um_config_get_file_size(file);
    if (fsize == -1)
        return NULL;

    /* Выделяем память для строки, в которую скопируется содержимое файла. */
    char *const src = malloc(fsize + 1);
    if (src == NULL)
    {
        fprintf(stderr, "Can't allocate %ld bytes for the text of the file.\n", fsize + 1);
        return NULL;
    }

    /* Записываем содержимое файла в строку. Добавляем нуль-терминатор. */
    const size_t read_size = fread(src, 1, fsize, file);
    src[read_size] = '\0';

    return src;
}

char *um_config_get_config_file_dio()
{
    cJSON *el = NULL;
    cJSON *do_array = NULL;
    cJSON *di_array = NULL;
    char *config = um_config_get_config_file(CONFIG_FILE_SENSORS);
    cJSON *root = cJSON_Parse(config);

    do_array = cJSON_GetObjectItemCaseSensitive(root, "do");
    di_array = cJSON_GetObjectItemCaseSensitive(root, "di");

    int8_t relays = um_nvs_read_i8(NVS_KEY_RELAYS);
    int8_t inputs = di_get_state();
    int i = 0;
    int level = 0;
    // TODO change i with CONFIG_UMNI_DI_
    cJSON_ArrayForEach(el, do_array)
    {
        level = ((relays >> i) & 0x01) == 0 ? 1 : 0;
        cJSON_AddNumberToObject(el, "state", level);
        i++;
    }
    i = 0;
    el = NULL;
    cJSON_ArrayForEach(el, di_array)
    {
        level = 0;
        level = ((inputs >> i) & 0x01) == 0 ? 0 : 1;
        cJSON_AddNumberToObject(el, "state", level);
        i++;
    }

    char *res = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(config);
    return res;
}