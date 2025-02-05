#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

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
        mkdir(fullpath, 0700);
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
    um_config_create_dir_if_not_exists(CONFIG_PATH);
    um_config_create_dir_if_not_exists(LOG_PATH);
    return ESP_OK;
}