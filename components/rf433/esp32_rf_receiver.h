#ifndef ESP32_RF_RECEIVER_H_
#define ESP32_RF_RECEIVER_H_

#include <stdint.h>
#include <stdbool.h>

// Number of maximum high/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync
#define RCSWITCH_MAX_CHANGES 67

/**
 * Description of a single pulse, which consists of a high signal
 * whose duration is "high" times the base pulse length, followed
 * by a low signal lasting "low" times the base pulse length.
 * Thus, the pulse overall lasts (high+low)*pulseLength
 */
typedef struct HighLow
{
    uint8_t high;
    uint8_t low;
} HighLow;

/**
 * A "protocol" describes how zero and one bits are encoded into high/low
 * pulses.
 */
typedef struct Protocol
{
    /** base pulse length in microseconds, e.g. 350 */
    uint16_t pulseLength;

    HighLow syncFactor;
    HighLow zero;
    HighLow one;

    /**
     * If true, interchange high and low logic levels in all transmissions.
     */
    bool invertedSignal;
} Protocol;

#endif /* MAIN_RXB6_RECEIVER_H_ */

unsigned long esp_rf433_get_received_value();
unsigned int esp_rf433_get_received_bit_length();
void esp_rf433_reset_available();
void esp_rf433_initialize(int pin, void *handler);

// Функции для калибровки
void esp_rf433_set_receive_tolerance(int tolerance);
void esp_rf433_set_separation_limit(unsigned limit);
int esp_rf433_get_receive_tolerance();
unsigned esp_rf433_get_separation_limit();