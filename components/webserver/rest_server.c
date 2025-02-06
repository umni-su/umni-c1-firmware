/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <esp_system.h>
#include <esp_log.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"

#include "cJSON.h"
#include "webserver.h"

#include "../../main/includes/events.h"
#include "../nvs/nvs.h"
#include "../systeminfo/systeminfo.h"
#include "../config/config.h"
#include "../dio/dio.h"

#define MAX_CLIENTS CONFIG_UMNI_WEB_MAX_CLIENTS

static const char *REST_TAG = "esp-rest";

static bool authenticated = false;

httpd_handle_t server = NULL;

uint8_t client_ids[MAX_CLIENTS] = {0};

static bool installed = false;

typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

int get_sockfd(httpd_req_t *req)
{
    return httpd_req_to_sockfd(req);
}

bool check_auth()
{
    return authenticated;
}

/**
 *  Set HTTP response content type according to file extension
 *
 * @param   char       filepath  [filepath description]
 *
 * @return  esp_err_t            [return description]
 */
static esp_err_t
set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "image/svg+xml";
    }
    return httpd_resp_set_type(req, type);
}

/**
 * Prepare buffer from post request
 *
 * @return  char    [return description]
 */
static char *prepare_post_buffer(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return NULL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return NULL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    return buf;
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));

    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        if (req->sess_ctx)
        {
            authenticated = *(bool *)req->sess_ctx;
        }
        else
        {
            authenticated = false;
        }

        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
        char *token = strtok(filepath, "?");
        if (token != NULL)
        {
            // printf(" %s\n", token);
        }
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1)
    {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do
    {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        }
        else if (read_bytes > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    // ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    ESP_LOGI(REST_TAG, "Free heap size before: %ld", esp_get_free_heap_size());
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();

    um_systeminfo_data_type_t info = um_systeminfo_get_struct_data();
    cJSON_AddStringToObject(root, "date", info.date);
    cJSON_AddStringToObject(root, "last_reset", info.last_reset);
    cJSON_AddNumberToObject(root, "reset_reason", (int)info.restart_reason);
    cJSON_AddNumberToObject(root, "uptime", (int)info.uptime);
    cJSON_AddNumberToObject(root, "free_heap", info.free_heap);
    cJSON_AddNumberToObject(root, "total_heap", info.total_heap);
    cJSON_AddStringToObject(root, "fw_ver", info.fw_ver);
    cJSON_AddStringToObject(root, "fw_ver_web", info.fw_ver_web);
    cJSON_AddNumberToObject(root, "chip", info.chip);
    cJSON_AddNumberToObject(root, "cores", info.cores);
    cJSON_AddNumberToObject(root, "revision", info.model);

    // Netif
    cJSON *netif = cJSON_CreateArray();
    // ETHERNET
    cJSON *eth = cJSON_CreateObject();
    cJSON_AddStringToObject(eth, "name", info.ip_eth_info.name);
    cJSON_AddStringToObject(eth, "mac", info.ip_eth_info.mac);
    cJSON_AddStringToObject(eth, "ip", info.ip_eth_info.ip);
    cJSON_AddStringToObject(eth, "mask", info.ip_eth_info.mask);
    cJSON_AddStringToObject(eth, "gw", info.ip_eth_info.gw);

    cJSON_AddItemToArray(netif, eth);
    cJSON_AddItemToObject(root, "netif", netif);

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    cJSON_Delete(root);
    free((void *)sys_info);
    ESP_LOGI(REST_TAG, "Free heap size after: %ld", esp_get_free_heap_size());
    return ESP_OK;
}

static esp_err_t adm_auth_ckeck(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    bool success = check_auth();
    cJSON_AddBoolToObject(response, "installed", installed); // is system installed or not
    cJSON_AddBoolToObject(response, "success", success);     // authenticated or not

    const char *json = cJSON_Print(response);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(response);
    return ESP_OK;
}

static esp_err_t adm_install(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    char *buf = prepare_post_buffer(req);
    bool success = false;
    if (buf != NULL)
    {
        cJSON *post = cJSON_Parse(buf);
        char *username = cJSON_GetObjectItem(post, "u")->valuestring;
        char *password = cJSON_GetObjectItem(post, "p")->valuestring;
        char *passwordRepeat = cJSON_GetObjectItem(post, "pr")->valuestring;
        if (strcmp(password, passwordRepeat) == 0)
        {
            // Save admin creds
            um_nvs_write_str(NVS_KEY_USERNAME, username);
            um_nvs_write_str(NVS_KEY_PASSWORD, password);
            // Set installed = true
            um_nvs_write_i8(NVS_KEY_INSTALLED, 1);
            // Fire event to start services!
            installed = true;
            esp_event_post(APP_EVENTS, EV_SYSTEM_INSTALLED, NULL, sizeof(NULL), portMAX_DELAY);

            success = true;
        }
        cJSON_Delete(post);
    }
    cJSON_AddBoolToObject(response, "success", success);

    const char *json = cJSON_Print(response);

    // httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, json);

    cJSON_Delete(response);
    free((void *)json);

    return ESP_OK;
}

static esp_err_t adm_auth_login(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    char *buf = prepare_post_buffer(req);
    bool success = false;
    if (buf != NULL)
    {
        cJSON *post = cJSON_Parse(buf);
        char *username = cJSON_GetObjectItem(post, "u")->valuestring;
        char *password = cJSON_GetObjectItem(post, "p")->valuestring;
        char *nvs_username = um_nvs_read_str(NVS_KEY_USERNAME);
        char *nvs_password = um_nvs_read_str(NVS_KEY_PASSWORD);
        if (strcmp(password, nvs_password) == 0 && strcmp(username, nvs_username) == 0)
        {
            authenticated = true;
            success = true;
        }
        cJSON_Delete(post);
    }
    cJSON_AddBoolToObject(response, "installed", installed);
    cJSON_AddBoolToObject(response, "success", success);

    const char *json = cJSON_Print(response);

    // httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, json);

    cJSON_Delete(response);
    free((void *)json);

    return ESP_OK;
}

static esp_err_t adm_auth_logout(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    authenticated = false;

    cJSON *response = cJSON_CreateObject();
    bool success = true;
    cJSON_AddBoolToObject(response, "success", success); // authenticated or not

    const char *json = cJSON_Print(response);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(response);
    return ESP_OK;
}

static esp_err_t adm_st_dio(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char *config = um_config_get_config_file_dio();
    httpd_resp_sendstr(req, config);
    free((void *)config);
    return ESP_OK;
}

/** POST to dio */
static esp_err_t system_info_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    bool success = false;
    char *buf = prepare_post_buffer(req);
    cJSON *res = cJSON_CreateObject();
    cJSON *data = cJSON_Parse(buf);

    char *type = cJSON_GetObjectItem(data, "type")->valuestring;

    cJSON *state = cJSON_GetObjectItem(data, "state");

    if (strcmp(type, "dio") == 0)
    {
        success = true;
        int index = cJSON_GetObjectItem(state, "index")->valueint;
        int value = cJSON_GetObjectItem(state, "state")->valueint;
        do_set_level(index, value);
    }

    cJSON_AddBoolToObject(res, "success", success);

    char *answer = cJSON_PrintUnformatted(res);

    httpd_resp_sendstr(req, answer);
    // free((void *)buf);
    free((void *)answer);
    cJSON_Delete(data);
    cJSON_Delete(res);
    return ESP_OK;
}

/* Custom function to free context */
void free_ctx_func(void *ctx)
{
    /* Could be something other than free */
    free(ctx);
}

esp_err_t start_rest_server(const char *base_path)
{
    authenticated = false;
    installed = um_nvs_is_installed();
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 50;
    config.lru_purge_enable = true;
    config.max_open_sockets = MAX_CLIENTS + 2;
    //---config.global_user_ctx = keep_alive;
    // config.open_fn = ws_open_fd;
    //---config.close_fn = ws_close_fd;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /** AUTH **/
    // Check auth
    httpd_uri_t adm_auth_ckeck_uri = {
        .uri = "/adm/auth/check",
        .method = HTTP_GET,
        .handler = adm_auth_ckeck,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_auth_ckeck_uri);

    // Login
    httpd_uri_t adm_auth_login_url = {
        .uri = "/adm/auth/login",
        .method = HTTP_POST,
        .handler = adm_auth_login,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_auth_login_url);

    // Logout
    httpd_uri_t adm_auth_logout_url = {
        .uri = "/adm/auth/logout",
        .method = HTTP_GET,
        .handler = adm_auth_logout,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_auth_logout_url);

    // Install ap
    httpd_uri_t install_uri = {
        .uri = "/adm/install",
        .method = HTTP_POST,
        .handler = adm_install,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &install_uri);

    /** STATES**/
    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/adm/st/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t adm_st_post_uri = {
        .uri = "/adm/st",
        .method = HTTP_POST,
        .handler = system_info_post_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_st_post_uri);

    // DIO get
    httpd_uri_t adm_st_dio_uri = {
        .uri = "/adm/st/dio",
        .method = HTTP_GET,
        .handler = adm_st_dio,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_st_dio_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    // if (err != ESP_OK)
    //{
    // ESP_LOGE(REST_TAG, "Error: %s", esp_err_to_name(err));
    //}

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
