#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mosquitto.h"

#include "../config/config.h"
#include "../../main/includes/events.h"
#include "../nvs/nvs.h"
#include "../systeminfo/systeminfo.h"

const char *MQTT_TAG = "mqtt";

static bool connected = false;
static bool enabled = false;
static char *url;
static int16_t port;
static char *username = NULL;
static char *password = NULL;

static char *name;

int msg_id;
esp_mqtt_event_handle_t event = NULL;
esp_mqtt_client_handle_t client = NULL;

static um_mqtt_status_t connection_status;

TaskHandle_t mqtt_register_handler = NULL;

bool success = false;

void um_mqtt_register_task(void *args)
{
    while (true)
    {
        if (!connected)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else
        {
            um_mqtt_register_device();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            um_mqtt_send_config();
            vTaskDelay(REGISTER_TIMEOUT / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(mqtt_register_handler);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void watch_events(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    cJSON *payload = cJSON_CreateObject();
    char *topic = NULL;
    switch (id)
    {
    case EV_STATUS_CHANGED_DI:
    case EV_STATUS_CHANGED_DO:
        topic = (id == EV_STATUS_CHANGED_DI) ? UM_TOPIC_INPUT : UM_TOPIC_RELAY;
        um_ev_message_dio *status_dio = (um_ev_message_dio *)event_data;

        cJSON_AddNumberToObject(payload, "index", status_dio->index);
        cJSON_AddNumberToObject(payload, "level", status_dio->level);

        break;
    case EV_STATUS_CHANGED_NTC:
        topic = UM_TOPIC_NTC;
        um_ev_message_ntc *status_ntc = (um_ev_message_ntc *)event_data;
        cJSON_AddNumberToObject(payload, "channel", status_ntc->channel);
        cJSON_AddNumberToObject(payload, "temp", status_ntc->temp);
        break;

    case EV_STATUS_CHANGED_OW:
        topic = UM_TOPIC_STATUS_ONEWIRE;
        um_ev_message_onewire *status_onewire = (um_ev_message_onewire *)event_data;

        cJSON_AddStringToObject(payload, "sn", status_onewire->sn);
        cJSON_AddNumberToObject(payload, "temp", status_onewire->temp);
        break;
    }

    char *json = cJSON_PrintUnformatted(payload);
    um_mqtt_publish_data(topic, json);
    free((void *)json);
    cJSON_Delete(payload);
    ESP_LOGW("HEAP", "[watch_events (mqtt)] Free memory: %ld bytes", esp_get_free_heap_size());
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    event = event_data;
    client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        connected = true;
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");

        xTaskCreatePinnedToCore(um_mqtt_register_task, "mqtt_register_task", configMINIMAL_STACK_SIZE * 2, NULL, 3, &mqtt_register_handler, 1);

        // SUBSCRIBE HERE!

        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(MQTT_TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(MQTT_TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        connected = false;
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(MQTT_TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }

    connection_status.success = connected;
}

void um_mqtt_init()
{
    name = um_nvs_read_str(NVS_KEY_MACNAME);
    if (name == NULL || strlen(name) < 4)
    { // UMNI
        ESP_LOGE(MQTT_TAG, "MQTT can not start, because NVS_KEY_MACNAME not exists in NVS!");
        return;
    }
    enabled = um_nvs_read_i8(NVS_KEY_MQTT_ENABLED) == 1;
    if (!enabled)
        return;
    url = um_nvs_read_str(NVS_KEY_MQTT_HOST);
    port = um_nvs_read_i16(NVS_KEY_MQTT_PORT);
    username = um_nvs_read_str(NVS_KEY_MQTT_USER);
    password = um_nvs_read_str(NVS_KEY_MQTT_PWD);

    connection_status.url = url;

    if (port < 1)
    {
        port = 1883;
    }
    size_t len = strlen(url);
    char address_with_protocol[len + 10];
    sprintf(address_with_protocol, "mqtt://%s", url);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = address_with_protocol,
        .broker.address.port = port};
    if (username != NULL && password != NULL)
    {
        mqtt_cfg.credentials.username = username;
        mqtt_cfg.credentials.client_id = name;
        mqtt_cfg.credentials.authentication.password = password;
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &watch_events, NULL));
}

char *um_mqtt_get_full_topic(char *topic)
{
    return NULL;
}

/**
 * Publish data always to /status/ topics
 */
esp_err_t um_mqtt_publish_data(char *topic, char *data)
{
    if (topic == NULL)
        return ESP_FAIL;
    size_t prefix_len = strcmp(topic, UM_TOPIC_REGISTER) == 0 ? 0 : strlen(UM_TOPIC_PREFIX_DEVICE);
    size_t len = strlen(topic) + strlen(name) + prefix_len + 1;
    char real_topic[len];
    sprintf(real_topic, "%s%s%s", UM_TOPIC_PREFIX_DEVICE, strcmp(topic, UM_TOPIC_REGISTER) == 0 ? "" : name, topic);
    esp_err_t res = ESP_OK;
    if (!connected)
    {
        return ESP_FAIL;
    }
    if (client != NULL)
    {

        ESP_LOGI("MQTT", "Send data to server: %s, %s", real_topic, data);
        msg_id = esp_mqtt_client_publish(client, real_topic, data, 0, 0, 0);
    }
    res = ESP_OK;
    return res;
}

esp_err_t um_mqtt_register_device()
{

    cJSON *request = cJSON_CreateObject();

    cJSON_AddStringToObject(request, "name", name);
    cJSON_AddNumberToObject(request, "type", DEVICE_UMNI_C_ONE);

    cJSON *systeminfo = cJSON_CreateObject();

    um_netif_data_type_t eth = um_systeminfo_get_eth_netif_config();

    // const um_systeminfo_data_type_t system_info = um_systeminfo_get_struct_data();

    // cJSON *systeminfo = cJSON_CreateObject();

    // cJSON_AddStringToObject(systeminfo, "date", system_info.date);
    // cJSON_AddStringToObject(systeminfo, "last_reset", system_info.last_reset);
    // cJSON_AddNumberToObject(systeminfo, "reset_reason", (int)system_info.restart_reason);
    // cJSON_AddNumberToObject(systeminfo, "uptime", system_info.uptime);
    // cJSON_AddNumberToObject(systeminfo, "free_heap", system_info.free_heap);
    // cJSON_AddNumberToObject(systeminfo, "total_heap", system_info.total_heap);
    // cJSON_AddStringToObject(systeminfo, "fw_ver", system_info.fw_ver);
    // cJSON_AddStringToObject(systeminfo, "fw_ver_web", system_info.fw_ver_web);
    // cJSON_AddNumberToObject(systeminfo, "chip", system_info.chip);
    // cJSON_AddNumberToObject(systeminfo, "cores", system_info.cores);
    // cJSON_AddNumberToObject(systeminfo, "revision", system_info.model);

    // // Netif
    cJSON *netif = cJSON_CreateArray();
    // // ETHERNET
    cJSON *ethernet = cJSON_CreateObject();
    cJSON_AddStringToObject(ethernet, "name", eth.name);
    cJSON_AddStringToObject(ethernet, "mac", eth.mac);
    cJSON_AddStringToObject(ethernet, "ip", eth.ip);
    cJSON_AddStringToObject(ethernet, "mask", eth.mask);
    cJSON_AddStringToObject(ethernet, "gw", eth.gw);

    cJSON_AddItemToArray(netif, ethernet);
    cJSON_AddItemToObject(systeminfo, "netif", netif);
    cJSON_AddItemToObject(request, "systeminfo", systeminfo);

    char *data = cJSON_PrintUnformatted(request);

    um_mqtt_publish_data(UM_TOPIC_REGISTER, data);

    free((void *)data);
    cJSON_Delete(request);

    return ESP_OK;
}

esp_err_t um_mqtt_send_config()
{
    char *config;
    cJSON *json_config;
    char *json_str;
    config = um_config_get_config_file_dio();
    json_config = cJSON_Parse(config);
    json_str = cJSON_PrintUnformatted(json_config);
    um_mqtt_publish_data(UM_TOPIC_CONFIGURATION_DIO, json_str);

    free(json_str);
    cJSON_Delete(json_config);
    free(config);

    config = um_config_get_config_file(CONFIG_FILE_ONEWIRE);
    json_config = cJSON_Parse(config);
    json_str = cJSON_PrintUnformatted(json_config);
    um_mqtt_publish_data(UM_TOPIC_CONFIGURATION_OW, json_str);

    free(json_str);
    cJSON_Delete(json_config);
    free(config);

    ESP_LOGW("HEAP", "[um_mqtt_send_config] Free memory: %ld bytes", esp_get_free_heap_size());

    return ESP_OK;
}

um_mqtt_status_t um_mqtt_get_connection_state()
{
    return connection_status;
}