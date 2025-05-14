// #pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
void um_am_test();
typedef enum
{
    UM_AM_TRIG_ALWAYS = 1,
    UM_AM_TRIG_EQUAL = 2,
    UM_AM_TRIG_MORE = 3,
    UM_AM_TRIG_LESS = 4,
} um_am_trigger_type_t;

typedef struct
{
    um_am_trigger_type_t cond;
    float value;
} um_am_trigger_t;

typedef enum
{
    UM_AM_RELAY = 1,
    UM_AM_BOILER = 2,
    UM_AM_MATRIX = 3
} um_am_action_type_t;

typedef enum
{
    ON = 1,
    OFF = 0
} um_am_boiler_ch_t;

typedef struct
{
    um_am_boiler_ch_t ch;
} um_am_action_boiler_t;

typedef struct
{
    int on[6];
    int off[6];
} um_am_action_relay_t;

typedef struct
{
    short int val;            // val of trigger sensor
    short int rel;            // relay index to manage
    bool inv;                 // inverse state or not
    unsigned short int state; // HIGH or LOW
} um_am_matrix_t;

typedef struct
{
    um_am_action_relay_t relay_action;
    um_am_action_boiler_t boiler_action;
    um_am_matrix_t matrix_action[6];
} um_am_options_t;

typedef struct
{
    bool ext;
    bool inverted;
    um_am_trigger_t trigger;
    um_am_options_t opts;
    float value; // target value (float or int)

} um_am_main_t;

void um_am_parse_json_config(cJSON *sensor_json, um_am_main_t *config);

bool um_am_automation_has_matrix(um_am_main_t *config);

bool um_am_automation_has_relays(um_am_main_t *config);

bool um_am_automation_has_boiler(um_am_main_t *config);

void um_am_automation_run(um_am_main_t *config);