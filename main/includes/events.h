#ifndef EVENT_SOURCE_H_
#define EVENT_SOURCE_H_

#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int index;
        int level;
    } um_ev_message_dio;

    typedef struct
    {
        int channel;
        float temp;
    } um_ev_message_ntc;

    typedef struct
    {
        int channel;
        int value;
        int voltage;
    } um_ev_message_ai;

    typedef struct
    {
        char *sn;
        float temp;
    } um_ev_message_onewire;

    typedef struct
    {
        uint32_t serial;
        uint8_t state;
        bool alarm;
        bool triggered;
    } um_ev_message_rf433;

    // Declare an event base
    ESP_EVENT_DECLARE_BASE(APP_EVENTS); // declaration of the timer events family

    enum
    {
        // declaration of the specific events under the timer event family
        EV_ETH_CONNECTED = 700,
        EV_ETH_DISCONNECTED,
        EV_ETH_START,
        EV_ETH_MAC,
        EV_SDCARD_MOUNTED,
        EV_CONFIGURATION_READY,
        EV_ETH_GOT_IP,
        EV_ETH_STOP,
        EV_WIFI_MODE_UPDATED,
        EV_NVS_WIFI_INIT_ON_STARTUP,
        EV_SPIFFS_READY,
        EV_NVS_OPENED,
        EV_DO_INIT,
        EV_ONEWIRE_INIT,
        EV_SYSTEM_INSTALLED,
        EV_NTP_SYNC_SUCCESS,
        EV_STATUS_CHANGED_DI,
        EV_STATUS_CHANGED_DO,
        EV_STATUS_CHANGED_NTC,
        EV_STATUS_CHANGED_AI,
        EV_STATUS_CHANGED_OW,
        EV_OTA_START,
        EV_OTA_SUCCESS,
        EV_OTA_ABORT,
        EV_OT_SET_DATA,
        EV_OT_CH_ON,
        EV_OT_CH_OFF,
        EV_RF433_SENSOR,
        EV_AUTOMATION_FIRED
    };

#ifdef __cplusplus
}
#endif

#endif // #ifndef EVENT_SOURCE_H_