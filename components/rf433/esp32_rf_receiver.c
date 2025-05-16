/**
 * Inspired by RC-Switch library (https://github.com/sui77/rc-switch)

 * Mac Wyznawca make some changes. Non-blocking loop with Queue and ESP-SDK native function esp_timer_get_time() for millisecond.

 */

#include "esp32_rf_receiver.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "esp_system.h"
#include "include/output.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#define ADVANCED_OUTPUT 1
#define TAG "RF433"

static QueueHandle_t s_esp_RF433_queue = NULL;

static const Protocol proto[] = {
    {350, {1, 31}, {1, 3}, {3, 1}, false},   // protocol 1
    {650, {1, 10}, {1, 2}, {2, 1}, false},   // protocol 2
    {100, {30, 71}, {4, 11}, {9, 6}, false}, // protocol 3
    {380, {1, 6}, {1, 3}, {3, 1}, false},    // protocol 4
    {500, {6, 14}, {1, 2}, {2, 1}, false},   // protocol 5
    {450, {23, 1}, {1, 2}, {2, 1}, true}     // protocol 6 (HT6P20B)
};

enum
{
  numProto = sizeof(proto) / sizeof(proto[0])
};

volatile unsigned long nReceivedValue = 0;
volatile unsigned int nReceivedBitlength = 0;
volatile unsigned int nReceivedDelay = 0;
volatile unsigned int nReceivedProtocol = 0;
int nReceiveTolerance = 60;
const unsigned nSeparationLimit = 4300;
// separationLimit: minimum microseconds between received codes, closer codes are ignored.
// according to discussion on issue #14 it might be more suitable to set the separation
// limit to the same time as the 'low' part of the sync signal for the current protocol.

unsigned int timings[RCSWITCH_MAX_CHANGES];

/* helper function for the esp_rf433_receive_protocol method */
static inline unsigned int diff(int A, int B)
{
  return abs(A - B);
}

bool esp_rf433_receive_protocol(const int p, unsigned int changeCount)
{
  const Protocol pro = proto[p - 1];

  unsigned long code = 0;
  // Assuming the longer pulse length is the pulse captured in timings[0]
  const unsigned int syncLengthInPulses = ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
  const unsigned int delay = timings[0] / syncLengthInPulses;
  const unsigned int delayTolerance = delay * nReceiveTolerance / 100;

  /* For protocols that start low, the sync period looks like
   *               _________
   * _____________|         |XXXXXXXXXXXX|
   *
   * |--1st dur--|-2nd dur-|-Start data-|
   *
   * The 3rd saved duration starts the data.
   *
   * For protocols that start high, the sync period looks like
   *
   *  ______________
   * |              |____________|XXXXXXXXXXXXX|
   *
   * |-filtered out-|--1st dur--|--Start data--|
   *
   * The 2nd saved duration starts the data
   */
  const unsigned int firstDataTiming = (pro.invertedSignal) ? (2) : (1);

  for (unsigned int i = firstDataTiming; i < changeCount - 1; i += 2)
  {
    code <<= 1;
    if (diff(timings[i], delay * pro.zero.high) < delayTolerance &&
        diff(timings[i + 1], delay * pro.zero.low) < delayTolerance)
    {
      // zero
    }
    else if (diff(timings[i], delay * pro.one.high) < delayTolerance &&
             diff(timings[i + 1], delay * pro.one.low) < delayTolerance)
    {
      // one
      code |= 1;
    }
    else
    {
      // Failed
      return false;
    }
  }

  if (changeCount > 7)
  { // ignore very short transmissions: no device sends them, so this must be noise
    nReceivedValue = code;
    nReceivedBitlength = (changeCount - 1) / 2;
    nReceivedDelay = delay;
    nReceivedProtocol = p;
    return true;
  }

  return false;
}

// -- Wrappers over variables which should be modified only internally

bool esp_rf433_available()
{
  return nReceivedValue != 0;
}

void esp_rf433_reset_available()
{
  nReceivedValue = 0;
}

unsigned long esp_rf433_get_received_value()
{
  return nReceivedValue;
}

unsigned int esp_rf433_get_received_bit_length()
{
  return nReceivedBitlength;
}

unsigned int esp_rf433_get_received_delay()
{
  return nReceivedDelay;
}

unsigned int esp_rf433_get_received_protocol()
{
  return nReceivedProtocol;
}

unsigned int *esp_rf433_get_received_raw_data()
{
  return timings;
}

void esp_rf433_interrupt_task(void *arg)
{
  static unsigned int changeCount = 0;
  static unsigned long lastTime = 0;
  static unsigned int repeatCount = 0;

  const long time = esp_timer_get_time();
  const unsigned int duration = time - lastTime;

  if (duration > nSeparationLimit)
  {
    // A long stretch without signal level change occurred. This could
    // be the gap between two transmission.
    if (diff(duration, timings[0]) < 200)
    {
      // This long signal is close in length to the long signal which
      // started the previously recorded timings; this suggests that
      // it may indeed by a a gap between two transmissions (we assume
      // here that a sender will send the signal multiple times,
      // with roughly the same gap between them).
      repeatCount++;
      if (repeatCount == 2)
      {
        for (uint8_t i = 1; i <= numProto; i++)
        {
          if (esp_rf433_receive_protocol(i, changeCount))
          {
            // receive succeeded for protocol i
            uint8_t protocol_num = (uint8_t)i;
            xQueueSendFromISR(s_esp_RF433_queue, &protocol_num, NULL);
            break;
          }
        }
        repeatCount = 0;
      }
    }
    changeCount = 0;
  }
  // detect overflow
  if (changeCount >= RCSWITCH_MAX_CHANGES)
  {
    changeCount = 0;
    repeatCount = 0;
  }

  timings[changeCount++] = duration;
  lastTime = time;

  vTaskDelete(NULL);
}

static void IRAM_ATTR esp_rf433_data_interrupt_handler(void *arg)
{
  xTaskCreate(esp_rf433_interrupt_task, "esp_rf433_task", 4096, NULL, 2, NULL);
}

void esp_rf433_initialize(int pin, void *handler)
{
  if (s_esp_RF433_queue == NULL)
  {
    s_esp_RF433_queue = xQueueCreate(1, sizeof(uint8_t));
    if (s_esp_RF433_queue != NULL)
    {
      // Configure the data input
      gpio_config_t data_pin_config = {
          .intr_type = GPIO_INTR_ANYEDGE,
          .mode = GPIO_MODE_INPUT,
          .pin_bit_mask = (1ULL << pin),
          .pull_up_en = GPIO_PULLUP_DISABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE};

      gpio_config(&data_pin_config);

      // Attach the interrupt handler
      gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
      gpio_isr_handler_add(pin, esp_rf433_data_interrupt_handler, NULL);

      xTaskCreatePinnedToCore(handler, "um_rf433", configMINIMAL_STACK_SIZE * 4, s_esp_RF433_queue, 4, NULL, 1);
    }
  }
}
