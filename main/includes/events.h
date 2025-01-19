#ifndef EVENT_SOURCE_H_
#define EVENT_SOURCE_H_

#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Declare an event base
    ESP_EVENT_DECLARE_BASE(APP_EVENTS); // declaration of the timer events family

    enum
    {
        // declaration of the specific events under the timer event family
        EV_ETH_CONNECTED = 700,
        EV_ETH_DISCONNECTED,
        EV_ETH_START,
        EV_ETH_STOP,
        EV_WIFI_MODE_UPDATED,
        EV_NVS_WIFI_INIT_ON_STARTUP,
        EV_SPIFFS_READY,
        EV_DO_INIT
    };

#ifdef __cplusplus
}
#endif

#endif // #ifndef EVENT_SOURCE_H_