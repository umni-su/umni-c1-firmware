#define NVS_NAMESPACE "nvs"
#define NVS_WIFI_MODE_AP 1
#define NVS_WIFI_MODE_STA 2
#define NVS_WIFI_MODE_APSTA 3
#define NVS_WIFI_MODE_DISABLED 4

#define NVS_KEY_INSTALLED "inst"
#define NVS_KEY_HOSTNAME "name"
#define NVS_KEY_USERNAME "admusr"
#define NVS_KEY_PASSWORD "admpwd"

/**
 * Режим станции (ESP32 подключается к существующей сети WIFI)
 */
#define NVS_KEY_WIFI_STA_SSID "stname"
#define NVS_KEY_WIFI_STA_PWD "stapwd"

/**
 * Настройки wifi адаптера для режима STA
 */
#define NVS_KEY_WIFI_TYPE "wt"      // DHCP (1) or STATIC (2)
#define NVS_KEY_WIFI_IP "wip"       // IP address
#define NVS_KEY_WIFI_NETMASK "wnm"  // Netmask
#define NVS_KEY_WIFI_GATEWAY "wgw"  // Gateway
#define NVS_KEY_WIFI_NETMASK "wdns" // Empty for AUTO (use system)

/**
 * ETHERNET
 */

/** MQTT */

/** REST API */