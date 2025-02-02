#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN)
#define SCRATCH_BUFSIZE (8192)
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#define MDNS_INSTANCE "esp home web server"

esp_err_t init_fs(void);

esp_err_t start_rest_server(const char *base_path);

esp_err_t init_fs(void);

void webserver_start_task(void *args);

void webserver_start();
