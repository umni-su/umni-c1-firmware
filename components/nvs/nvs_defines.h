#define NVS_NAMESPACE "nvs"

#define NVS_KEY_INSTALLED "inst"
#define NVS_KEY_HOSTNAME "name"
#define NVS_KEY_MACNAME "macname"
#define NVS_KEY_USERNAME "admusr"
#define NVS_KEY_PASSWORD "admpwd"
#define NVS_KEY_NTP "ntp"
#define NVS_KEY_UPDATES_CHANNEL "upd"
#define NVS_KEY_TIMEZONE "tz"
#define NVS_KEY_POWERON_AT "poweron"
#define NVS_KEY_RESET_AT "resetat"

/**
 * Режим станции (ESP32 подключается к существующей сети WIFI)
 */
#define NVS_KEY_WIFI_STA_MAC "stmac"
#define NVS_KEY_WIFI_STA_SSID "stname"
#define NVS_KEY_WIFI_STA_PWD "stpwd"

/**
 * Настройки wifi адаптера для режима STA
 */
#define NVS_KEY_WIFI_MAC "wmac"
#define NVS_KEY_WIFI_TYPE "wt"     // Disable (NONE or 0) DHCP (1) or STATIC (2)
#define NVS_KEY_WIFI_IP "wip"      // IP address
#define NVS_KEY_WIFI_NETMASK "wnm" // Netmask
#define NVS_KEY_WIFI_GATEWAY "wgw" // Gateway
#define NVS_KEY_WIFI_DNS "wdns"    // Empty for AUTO (use system)

/**
 * ETHERNET
 */
#define NVS_KEY_ETH_MAC "emac"    // DHCP (1) or STATIC (2)
#define NVS_KEY_ETH_TYPE "et"     // DHCP (1) or STATIC (2)
#define NVS_KEY_ETH_IP "eip"      // IP address
#define NVS_KEY_ETH_NETMASK "enm" // Netmask
#define NVS_KEY_ETH_GATEWAY "egw" // Gateway
#define NVS_KEY_ETH_DNS "edns"    // Empty for AUTO (use system)

/**
 * Opentherm values
 */
#define NVS_KEY_OT_ENABLED "oten"
#define NVS_KEY_OT_DHW_SETPOINT "otdhwsp"
#define NVS_KEY_OT_TB_SETPOINT "ottbsp"

/**
 * Relays
 */
#define NVS_KEY_RELAYS "relays"

/** MQTT */
#define NVS_KEY_MQTT_ENABLED "mqen"
#define NVS_KEY_MQTT_HOST "mqhost"
#define NVS_KEY_MQTT_PORT "mqport"
#define NVS_KEY_MQTT_USER "mquser"
#define NVS_KEY_MQTT_PWD "mqpwd"

/** WEBHOOKS */
#define NVS_KEY_WEBHOOKS "whk"        // 0 (disabled) or 1 (enabled)
#define NVS_KEY_WEBHOOKS_URL "whkurl" // URL to send webhooks

typedef enum
{
    UM_IP_TYPE_NONE = 0,
    UM_IP_TYPE_DHCP,
    UM_IP_TYPE_STATIC
} um_nvs_ip_type_t;

typedef enum
{
    UM_UPD_CHAN_STABLE = 1,
    UM_UPD_CHAN_BETA
} um_nvs_update_channel_t;
