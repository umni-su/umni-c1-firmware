#include "../../main/includes/device_types.h"

#define REGISTER_TIMEOUT 10000

#define UM_TOPIC_PREFIX_MANAGE "manage/"
#define UM_TOPIC_PREFIX_DEVICE "device/"

/**
 * MQTT AVAILABLE TOPICS
 */
#define UM_TOPIC_REGISTER "register" // Device registration topic  NO / SLASH!!!
#define UM_TOPIC_REGISTERED "/registered"
#define UM_TOPIC_CONFIGURATION "/cnf" // Device send configuration topic
#define UM_TOPIC_CONFIGURATION_DIO "/cnf/dio"
#define UM_TOPIC_CONFIGURATION_OW "/cnf/ow"
#define UM_TOPIC_STATUS "/st" // Device send status topic
#define UM_TOPIC_STATUS_DIO "/dio"
#define UM_TOPIC_STATUS_ONEWIRE "/ow"
#define UM_TOPIC_RELAY "/rel"       // Device send relay status topic
#define UM_TOPIC_INPUT "/inp"       // Device send input status topic
#define UM_TOPIC_NTC "/ntc"         // Device send ntc sensors values topic
#define UM_TOPIC_OPENTHERM "/ot"    // Device send opentherm stztus topic
#define UM_TOPIC_ANALOG_INPUT "/ai" // Device registration topic
#define UM_TOPIC_NETWORK "/nw"      // Device registration topic

// cntrl/register
// cntrl/{NAMEMAC}/st
// cntrl/{NAMEMAC}/st/rel
// cntrl/{NAMEMAC}/st/inp
// cntrl/{NAMEMAC}/st/ntc
// cntrl/{NAMEMAC}/st/ai
// cntrl/{NAMEMAC}/st/nw

typedef struct
{
    char *name;
    umni_ecosystem_device_t type;
} um_mqtt_device_register_request_t;

typedef struct
{
    bool success;
    char *url;
} um_mqtt_status_t;

void um_mqtt_init();

esp_err_t um_mqtt_publish_data(char *topic, char *data);

char *um_mqtt_get_full_topic(char *topic);

/**
 * Send register to UMNI ECO
 */
esp_err_t um_mqtt_register_device();

/**
 * Send configuration to UMNI ECO
 */
esp_err_t um_mqtt_send_config();

um_mqtt_status_t um_mqtt_get_connection_state();