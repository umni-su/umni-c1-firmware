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
#include "../opentherm/opentherm_operations.h"
#include "../dio/dio.h"
#include "../adc/adc.h"
#include "../ota/ota.h"
#include "../mdns_service/mdns_service.h"

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
    ESP_LOGW(REST_TAG, "Free heap size before: %ld", esp_get_free_heap_size());
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();

    const um_systeminfo_data_type_t info = um_systeminfo_get_struct_data();
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
    free((void *)sys_info);
    cJSON_Delete(root);
    ESP_LOGW(REST_TAG, "Free heap size after: %ld", esp_get_free_heap_size());
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

static esp_err_t adm_st_ot(httpd_req_t *req)
{
    um_ot_data_t data = um_ot_get_data();
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", data.status);
    cJSON_AddNumberToObject(root, "otch", data.otch);
    cJSON_AddNumberToObject(root, "ottbsp", data.ottbsp);
    cJSON_AddNumberToObject(root, "otdhwsp", data.otdhwsp);
    cJSON_AddNumberToObject(root, "central_heating_active", data.central_heating_active);
    cJSON_AddNumberToObject(root, "hot_water_active", data.hot_water_active);
    cJSON_AddNumberToObject(root, "flame_on", data.flame_on);
    cJSON_AddNumberToObject(root, "modulation", data.modulation);
    cJSON_AddNumberToObject(root, "pressure", data.pressure);
    cJSON_AddNumberToObject(root, "slave_ot_version", data.slave_ot_version);
    cJSON_AddNumberToObject(root, "slave_product_version", data.slave_product_version);
    cJSON_AddNumberToObject(root, "dhw_temperature", data.dhw_temperature);
    cJSON_AddNumberToObject(root, "boiler_temperature", data.boiler_temperature);
    cJSON_AddNumberToObject(root, "return_temperature", data.return_temperature);
    cJSON_AddBoolToObject(root, "fault", data.is_fault);
    cJSON_AddNumberToObject(root, "fault_code", data.fault_code);
    cJSON_AddNumberToObject(root, "ntc1", get_ntc_data_channel_temp(AN_NTC_1));
    cJSON_AddNumberToObject(root, "ntc2", get_ntc_data_channel_temp(AN_NTC_2));

    char *res = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, res);
    free((void *)res);
    cJSON_Delete(root);
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

    if (strcmp(type, "ot") == 0)
    {
        // otch: true
        // otdhwsp: 45
        // ottbsp:55
        success = true;
        cJSON *ch = cJSON_GetObjectItem(state, "otch");
        bool otch = cJSON_IsTrue(ch);
        int otdhwsp = cJSON_GetObjectItem(state, "otdhwsp")->valueint;
        int ottbsp = cJSON_GetObjectItem(state, "ottbsp")->valueint;

        um_ot_update_state(otch, otdhwsp, ottbsp);
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

/** GET to settings configuration*/
static esp_err_t adm_settings(httpd_req_t *req)
{
    cJSON *config = cJSON_CreateObject();

    char *dio_config = um_config_get_config_file_dio();
    char *one_wire_config = um_config_get_config_file(CONFIG_FILE_ONEWIRE);

    char *name = um_nvs_read_str(NVS_KEY_HOSTNAME);
    char *adm = um_nvs_read_str(NVS_KEY_USERNAME);
    int upd = um_nvs_read_i8(NVS_KEY_UPDATES_CHANNEL);
    char *tz = um_nvs_read_str(NVS_KEY_TIMEZONE);
    char *ntp = um_nvs_read_str(NVS_KEY_NTP);

    cJSON *c_system = cJSON_CreateObject();
    cJSON_AddStringToObject(c_system, NVS_KEY_HOSTNAME, name);
    cJSON_AddNumberToObject(c_system, NVS_KEY_UPDATES_CHANNEL, upd);
    cJSON_AddStringToObject(c_system, NVS_KEY_TIMEZONE, tz);
    cJSON_AddStringToObject(c_system, NVS_KEY_NTP, ntp);
    cJSON_AddStringToObject(c_system, NVS_KEY_USERNAME, adm);

    cJSON *c_network = cJSON_CreateObject();

    int et = um_nvs_read_i8(NVS_KEY_ETH_TYPE);
    char *eip = um_nvs_read_str(NVS_KEY_ETH_IP);
    char *enm = um_nvs_read_str(NVS_KEY_ETH_NETMASK);
    char *egw = um_nvs_read_str(NVS_KEY_ETH_GATEWAY);
    char *edns = um_nvs_read_str(NVS_KEY_ETH_DNS);
    cJSON_AddNumberToObject(c_network, NVS_KEY_ETH_TYPE, et);
    cJSON_AddStringToObject(c_network, NVS_KEY_ETH_IP, eip);
    cJSON_AddStringToObject(c_network, NVS_KEY_ETH_NETMASK, enm);
    cJSON_AddStringToObject(c_network, NVS_KEY_ETH_GATEWAY, egw);
    cJSON_AddStringToObject(c_network, NVS_KEY_ETH_DNS, edns);

    int wt = um_nvs_read_i8(NVS_KEY_WIFI_TYPE);
    char *wip = um_nvs_read_str(NVS_KEY_WIFI_IP);
    char *wnm = um_nvs_read_str(NVS_KEY_WIFI_NETMASK);
    char *wgw = um_nvs_read_str(NVS_KEY_WIFI_GATEWAY);
    char *wdns = um_nvs_read_str(NVS_KEY_WIFI_DNS);
    cJSON_AddNumberToObject(c_network, NVS_KEY_WIFI_TYPE, wt);
    cJSON_AddStringToObject(c_network, NVS_KEY_WIFI_IP, wip);
    cJSON_AddStringToObject(c_network, NVS_KEY_WIFI_NETMASK, wnm);
    cJSON_AddStringToObject(c_network, NVS_KEY_WIFI_GATEWAY, wgw);
    cJSON_AddStringToObject(c_network, NVS_KEY_WIFI_DNS, wdns);

    cJSON *c_api = cJSON_CreateObject();

    int mqen = um_nvs_read_i8(NVS_KEY_MQTT_ENABLED);
    char *mqhost = um_nvs_read_str(NVS_KEY_MQTT_HOST);
    int mqport = um_nvs_read_i16(NVS_KEY_MQTT_PORT);
    char *mquser = um_nvs_read_str(NVS_KEY_MQTT_USER);
    char *mqpwd = um_nvs_read_str(NVS_KEY_MQTT_PWD);
    int whk = um_nvs_read_i8(NVS_KEY_WEBHOOKS);
    char *whkuri = um_nvs_read_str(NVS_KEY_WEBHOOKS_URL);

    cJSON_AddNumberToObject(c_api, NVS_KEY_MQTT_ENABLED, mqen);
    cJSON_AddStringToObject(c_api, NVS_KEY_MQTT_HOST, mqhost);
    cJSON_AddNumberToObject(c_api, NVS_KEY_MQTT_PORT, mqport);
    cJSON_AddStringToObject(c_api, NVS_KEY_MQTT_USER, mquser);
    cJSON_AddStringToObject(c_api, NVS_KEY_MQTT_PWD, mqpwd);

    cJSON_AddNumberToObject(c_api, NVS_KEY_WEBHOOKS, whk);
    cJSON_AddStringToObject(c_api, NVS_KEY_WEBHOOKS_URL, whkuri);

    cJSON *c_dio = cJSON_Parse(dio_config);

    cJSON_AddItemToObject(config, "system", c_system);
    cJSON_AddItemToObject(config, "network", c_network);
    cJSON_AddItemToObject(config, "do", cJSON_GetObjectItem(c_dio, "do"));
    cJSON_AddItemToObject(config, "di", cJSON_GetObjectItem(c_dio, "di"));
    cJSON_AddItemToObject(config, "api", c_api);

    if (one_wire_config != NULL)
    {
        cJSON *ow = cJSON_Parse(one_wire_config);
        cJSON_AddItemToObject(config, "ow", ow);
    }

    char *res = cJSON_PrintUnformatted(config);
    httpd_resp_sendstr(req, res);
    free((void *)dio_config);
    free((void *)one_wire_config);
    free((void *)res);
    cJSON_Delete(config);

    return ESP_OK;
}

/** POST save settings */
static esp_err_t adm_settings_save(httpd_req_t *req)
{
    char *buff = prepare_post_buffer(req);
    cJSON *post = cJSON_Parse(buff);

    cJSON *res = cJSON_CreateObject();
    bool success = false;

    char *type = cJSON_GetObjectItem(post, "type")->valuestring;

    cJSON *data = cJSON_GetObjectItem(post, "data");

    if (strcmp(type, "system") == 0)
    {
        char *name = cJSON_GetObjectItem(data, "name")->valuestring;
        char *ntp = cJSON_GetObjectItem(data, "ntp")->valuestring;
        char *tz = cJSON_GetObjectItem(data, "tz")->valuestring;
        int upd = cJSON_GetObjectItem(data, "upd")->valueint;

        bool has_username = cJSON_HasObjectItem(data, "admuser");
        bool has_password = cJSON_HasObjectItem(data, "pwd");

        if (has_username && has_password)
        {
            char *admuser = cJSON_GetObjectItem(data, "admuser")->valuestring;
            char *pwd = cJSON_GetObjectItem(data, "pwd")->valuestring;

            um_nvs_write_str(NVS_KEY_USERNAME, admuser);
            um_nvs_write_str(NVS_KEY_PASSWORD, pwd);
        }

        um_nvs_write_str(NVS_KEY_HOSTNAME, name);
        um_nvs_write_str(NVS_KEY_NTP, ntp);
        um_nvs_write_str(NVS_KEY_TIMEZONE, tz);
        um_nvs_write_i8(NVS_KEY_UPDATES_CHANNEL, upd);

        success = true;
    }

    if (strcmp(type, "network") == 0)
    {
        int et = cJSON_GetObjectItem(data, "et")->valueint;
        char *eip = cJSON_HasObjectItem(data, "eip") ? cJSON_GetObjectItem(data, "eip")->valuestring : NULL;
        char *enm = cJSON_HasObjectItem(data, "enm") ? cJSON_GetObjectItem(data, "enm")->valuestring : NULL;
        char *egw = cJSON_HasObjectItem(data, "egw") ? cJSON_GetObjectItem(data, "egw")->valuestring : NULL;
        char *edns = cJSON_HasObjectItem(data, "edns") ? cJSON_GetObjectItem(data, "edns")->valuestring : NULL;

        int wt = cJSON_GetObjectItem(data, "wt")->valueint;
        char *wip = cJSON_HasObjectItem(data, "wip") ? cJSON_GetObjectItem(data, "wip")->valuestring : NULL;
        char *wnm = cJSON_HasObjectItem(data, "wnm") ? cJSON_GetObjectItem(data, "wnm")->valuestring : NULL;
        char *wgw = cJSON_HasObjectItem(data, "wgw") ? cJSON_GetObjectItem(data, "wgw")->valuestring : NULL;
        char *wdns = cJSON_HasObjectItem(data, "wdns") ? cJSON_GetObjectItem(data, "wdns")->valuestring : NULL;

        char *stname = cJSON_HasObjectItem(data, "stname") ? cJSON_GetObjectItem(data, "stname")->valuestring : NULL;
        char *stpwd = cJSON_HasObjectItem(data, "stname") ? cJSON_GetObjectItem(data, "stname")->valuestring : NULL;

        um_nvs_write_i8(NVS_KEY_ETH_TYPE, et);
        if (eip)
            um_nvs_write_str(NVS_KEY_ETH_IP, eip);
        if (enm)
            um_nvs_write_str(NVS_KEY_ETH_NETMASK, enm);
        if (egw)
            um_nvs_write_str(NVS_KEY_ETH_GATEWAY, egw);
        if (edns)
            um_nvs_write_str(NVS_KEY_ETH_DNS, edns);

        um_nvs_write_i8(NVS_KEY_WIFI_TYPE, wt);
        if (wip)
            um_nvs_write_str(NVS_KEY_WIFI_IP, wip);
        if (wnm)
            um_nvs_write_str(NVS_KEY_WIFI_NETMASK, wnm);
        if (wgw)
            um_nvs_write_str(NVS_KEY_WIFI_GATEWAY, wgw);
        if (wdns)
            um_nvs_write_str(NVS_KEY_WIFI_DNS, wdns);

        if (stname)
            um_nvs_write_str(NVS_KEY_WIFI_STA_SSID, stname);
        if (stpwd)
            um_nvs_write_str(NVS_KEY_WIFI_STA_PWD, stpwd);

        success = true;
    }

    if (strcmp(type, "sensors") == 0)
    {
        success = um_config_write_config_file(CONFIG_FILE_SENSORS, data);
    }

    if (strcmp(type, "api") == 0)
    {
        short int mqen = cJSON_GetObjectItem(data, "mqen")->valueint;
        char *mqhost = cJSON_HasObjectItem(data, "mqhost") ? cJSON_GetObjectItem(data, "mqhost")->valuestring : NULL;
        int mqport = cJSON_HasObjectItem(data, "mqport") ? cJSON_GetObjectItem(data, "mqport")->valueint : 0;
        char *mquser = cJSON_HasObjectItem(data, "mquser") ? cJSON_GetObjectItem(data, "mquser")->valuestring : NULL;
        char *mqpwd = cJSON_HasObjectItem(data, "mqpwd") ? cJSON_GetObjectItem(data, "mqpwd")->valuestring : NULL;

        short int whk = cJSON_GetObjectItem(data, "whk")->valueint;
        char *whkurl = cJSON_HasObjectItem(data, "whkurl") ? cJSON_GetObjectItem(data, "whkurl")->valuestring : NULL;

        if (mqen == 1)
        {
            um_nvs_write_i8(NVS_KEY_MQTT_ENABLED, (int8_t)1);
            if (mqhost)
                um_nvs_write_str(NVS_KEY_MQTT_HOST, mqhost);
            if (mqport)
                um_nvs_write_i16(NVS_KEY_MQTT_PORT, mqport);
            if (mquser)
                um_nvs_write_str(NVS_KEY_MQTT_USER, mquser);
            if (mqpwd)
                um_nvs_write_str(NVS_KEY_MQTT_PWD, mqpwd);
        }
        else
        {
            um_nvs_write_i8(NVS_KEY_MQTT_ENABLED, 0);
        }

        if (whk == 1)
        {
            um_nvs_write_i8(NVS_KEY_WEBHOOKS, 1);
            um_nvs_write_str(NVS_KEY_WEBHOOKS_URL, whkurl);
        }
        else
        {
            um_nvs_write_i8(NVS_KEY_WEBHOOKS, 0);
        }
    }

    cJSON_AddBoolToObject(res, "success", success);

    char *json = cJSON_PrintUnformatted(res);
    httpd_resp_sendstr(req, json);

    free((void *)json);
    cJSON_Delete(post);
    cJSON_Delete(res);
    return ESP_OK;
}

esp_err_t adm_update(httpd_req_t *req)
{
    bool success = false;
    char *data = prepare_post_buffer(req);
    cJSON *root = cJSON_Parse(data);
    const char *url = cJSON_HasObjectItem(root, "url") ? cJSON_GetObjectItem(root, "url")->valuestring : NULL;

    cJSON *response = cJSON_CreateObject();

    if (url != NULL)
    {
        um_ota_init();
        success = true;
    }
    cJSON_AddBoolToObject(response, "success", success);
    char *json = cJSON_PrintUnformatted(response);
    httpd_resp_sendstr(req, json);

    free((void *)json);
    cJSON_Delete(root);
    cJSON_Delete(response);
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
    um_mdns_prepare();
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
    //  config.open_fn = ws_open_fd;
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

    // DIO get
    httpd_uri_t adm_st_ot_uri = {
        .uri = "/adm/st/ot",
        .method = HTTP_GET,
        .handler = adm_st_ot,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_st_ot_uri);

    // adm/settings get
    httpd_uri_t adm_settings_uri = {
        .uri = "/adm/settings",
        .method = HTTP_GET,
        .handler = adm_settings,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_settings_uri);

    // adm/settings get
    httpd_uri_t adm_settings_post_uri = {
        .uri = "/adm/settings",
        .method = HTTP_POST,
        .handler = adm_settings_save,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_settings_post_uri);

    // adm/settings get
    httpd_uri_t adm_update_uri = {
        .uri = "/adm/update",
        .method = HTTP_POST,
        .handler = adm_update,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &adm_update_uri);

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
