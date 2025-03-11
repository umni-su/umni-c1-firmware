#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>

#define I2C_DO_ADDR CONFIG_UMNI_DO_I2C_ADDR

#define I2C_DI_ADDR CONFIG_UMNI_DI_I2C_ADDR

typedef enum
{
    DO_1 = CONFIG_UMNI_DO_CHAN_1,
    DO_2 = CONFIG_UMNI_DO_CHAN_2,
    DO_3 = CONFIG_UMNI_DO_CHAN_3,
    DO_4 = CONFIG_UMNI_DO_CHAN_4,
    DO_5 = CONFIG_UMNI_DO_CHAN_5,
    DO_6 = CONFIG_UMNI_DO_CHAN_6,
    LED_STAT = CONFIG_UMNI_STAT_LED,
    LED_ERR = CONFIG_UMNI_ERR_LED
} do_port_index_t;

typedef enum
{
    DI_1 = CONFIG_UMNI_DI_CHAN_1,
    DI_2 = CONFIG_UMNI_DI_CHAN_2,
    DI_3 = CONFIG_UMNI_DI_CHAN_3,
    DI_4 = CONFIG_UMNI_DI_CHAN_4,
    DI_5 = CONFIG_UMNI_DI_CHAN_5,
    DI_6 = CONFIG_UMNI_DI_CHAN_6,
    DI_FN = CONFIG_UMNI_FN_BTN,
    DI_CFG = CONFIG_UMNI_CFG_INP
} di_port_index_t;

typedef enum
{
    DO_HIGH = 1,
    DO_LOW = 0
} do_level_t;

typedef struct
{
    di_port_index_t chan;
    bool active;
    short int timeout;
    short int count;
    short int pause;
} led_blink_t;

/** Struct DI relay automation */
typedef struct
{
    bool ext;
    di_port_index_t on[6];
    di_port_index_t off[6];
} di_automation_relay_config_t;
/** END Struct DI relay automation */

typedef struct
{
    bool ext;
    bool ch;

} di_automation_boiler_config_t;

esp_err_t init_do();

do_level_t do_get_level(do_port_index_t channel);

esp_err_t do_set_level(do_port_index_t channel, do_level_t level);

void do_blink_led_err_init();

esp_err_t do_restore_all_values();

esp_err_t do_set_all_ff();

void do_blink_led_stat_start_at_boot();

void do_blink_led_stat_start_working();

void do_blink_led_stat_start_flashing();

void do_blink_led_error();

void do_blink_led_start_task(void *arg);

void do_blink_led_stop(do_port_index_t channel);

void do_blink_led_err_stop();

void do_blink_led_stat_stop();

esp_err_t do_register_events();

esp_err_t init_di();

void di_interrupt_task(void *arg);

uint8_t do_get_nvs_state();

esp_err_t do_set_nvs_state();

int8_t di_get_state();