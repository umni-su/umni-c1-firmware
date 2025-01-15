#ifndef EVENT_SOURCE_H_
#define EVENT_SOURCE_H_

#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Declare an event base
    ESP_EVENT_DECLARE_BASE(APP_EVENTS); // declaration of the timer events family
    // 0x140 = 320
    enum
    {                                      // declaration of the specific events under the timer event family
        UE_ETH_CONNECTED = 0x140,    // raised when the timer is first started
        UE_ETH_DISCONNECTED, // raised when a period of the timer has elapsed
        UE_ETH_EVENT_START,        // raised when the timer has been stopped,
        UE_ETH_EVENT_STOP,
        UE_WIFI_MODE_UPDATED,
        UE_NVS_INITIALIZED,
        UE_SPIFFS_READY
    };

#ifdef __cplusplus
}
#endif

#endif // #ifndef EVENT_SOURCE_H_