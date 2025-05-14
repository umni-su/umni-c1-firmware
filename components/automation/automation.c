#include "esp_log.h"
#include "esp_err.h"
#include "automation.h"
#include "../../main/includes/events.h"
#include "../dio/dio.h"
/**
 * All configuration of automation must have the same structure
 *
  "ext": true,
  "opt": {
    "trigger": {
      "cond": "always",
      "value": null
    },
    "actions": [
      {
        "type": 2,
        "ch": 1
      },
      {
        "type": 1,
        "on": [
          1,
          3
        ],
        "off": [
          0,
          4
        ]
      },
      {
      "type": 3,
      "matrix": [
          {
              "val": 1,
              "rel": 5,
              "inv": true,
              "state": 1
          },
          {
              "val": 2,
              "rel": 4,
              "inv": true,
              "state": 1
          },
          {
              "val": 4,
              "rel": 3,
              "inv": true,
              "state": 1
          },
          {
              "val": 8,
              "rel": 2,
              "inv": true,
              "state": 1
          }
      ]}
    ]
  }
 */

/**
 * Fill automation config by sensor json value
 * IMPORTANT!!! cJSON_Delete must called in parent method
 *
 * @return  void
 */
void um_am_parse_json_config(cJSON *sensor_json, um_am_main_t *config)
{
  // ESP_LOGW("um_am_parse_json_config", "%s", cJSON_Print(sensor_json));
  for (int j = 0; j < 6; j++)
  {
    config->opts.relay_action.off[j] = -1;
    config->opts.relay_action.on[j] = -1;

    // reset matrix actions to -1
    config->opts.matrix_action[j].inv = false;
    config->opts.matrix_action[j].rel = -1;
    config->opts.matrix_action[j].state = 0;
    config->opts.matrix_action[j].val = -1;
  }
  config->opts.boiler_action.ch = -1;
  // json foreach
  const cJSON *ext = cJSON_GetObjectItem(sensor_json, "ext");
  const cJSON *opt = cJSON_GetObjectItem(sensor_json, "opt");
  if (ext != NULL && opt != NULL)
  {
    config->ext = cJSON_IsTrue(ext);
    // Parse trigger
    cJSON *trigger = cJSON_GetObjectItem(opt, "trigger");
    if (trigger != NULL)
    {
      /** ============= TRIGGER ========================== **/
      cJSON *cond = cJSON_GetObjectItem(trigger, "cond");
      cJSON *value = cJSON_GetObjectItem(trigger, "value");
      config->trigger.cond = (um_am_trigger_type_t)cond->valueint;
      config->trigger.value = value->valuedouble;
      /** ============= TRIGGER ========================== **/

      // If trigger ok, parse actions
      cJSON *actions = cJSON_GetObjectItem(opt, "actions");
      if (actions != NULL && cJSON_IsArray(actions))
      {
        cJSON *action = NULL;
        cJSON_ArrayForEach(action, actions)
        {
          cJSON *type = cJSON_GetObjectItem(action, "type");
          switch (type->valueint)
          {
          /** ============= RELAYS ACTIONS ========================== **/
          case UM_AM_RELAY:
            cJSON *on = cJSON_GetObjectItem(action, "on");
            cJSON *off = cJSON_GetObjectItem(action, "off");
            cJSON *relay_index = NULL;
            unsigned short int i = 0;
            if (on != NULL && cJSON_IsArray(on))
            {
              cJSON_ArrayForEach(relay_index, on)
              {
                config->opts.relay_action.on[relay_index->valueint] = relay_index->valueint;
                i++;
              }
            }

            i = 0;
            relay_index = NULL;

            if (off != NULL && cJSON_IsArray(off))
            {
              cJSON_ArrayForEach(relay_index, off)
              {
                config->opts.relay_action.off[relay_index->valueint] = relay_index->valueint;
                i++;
              }
            }
            break;
          /** ============= RELAYS ACTIONS ========================== **/

          /** ============= BOILER ACTIONS ========================== **/
          case UM_AM_BOILER:
            cJSON *ch = cJSON_GetObjectItem(action, "ch");
            config->opts.boiler_action.ch = ch->valueint;
            break;

          case UM_AM_MATRIX:
            cJSON *matrix = cJSON_GetObjectItem(action, "matrix");
            if (matrix != NULL && cJSON_IsArray(matrix))
            {
              cJSON *matrix_el = NULL;
              unsigned short int matrix_loop_index = 0;
              cJSON_ArrayForEach(matrix_el, matrix)
              {
                cJSON *val = cJSON_GetObjectItem(matrix_el, "val");
                cJSON *rel = cJSON_GetObjectItem(matrix_el, "rel");
                if (val != NULL && rel != NULL)
                {
                  int value = val->valueint;
                  int relay = rel->valueint;
                  if (relay > -1 && (value > -1 && value <= 15))
                  { // 15 is 1111 max bit mask (4 channels)
                    cJSON *inv = cJSON_GetObjectItem(matrix_el, "inv");
                    cJSON *state = cJSON_GetObjectItem(matrix_el, "state");
                    if (inv != NULL && cJSON_IsBool(inv) && state != NULL)
                    {
                      bool inverse = cJSON_IsTrue(inv);
                      unsigned short int level = state->valueint;
                      config->opts.matrix_action[matrix_loop_index].val = value;
                      config->opts.matrix_action[matrix_loop_index].inv = inverse;
                      config->opts.matrix_action[matrix_loop_index].rel = relay;
                      config->opts.matrix_action[matrix_loop_index].state = level;
                    }
                  }
                }
                matrix_loop_index++;
              }
            }
            break;
            /** ============= BOILER ACTIONS ========================== **/
          default:
            break;
          }
        }
      }
    }
  }
}

bool um_am_automation_has_boiler(um_am_main_t *config)
{
  return config->ext && config->opts.boiler_action.ch > -1;
}

bool um_am_automation_has_relays(um_am_main_t *config)
{
  bool ok = config->ext;
  if (ok)
  {
    for (int i = 0; i < 6; i++)
    {
      if (config->opts.relay_action.off[i] > -1 || config->opts.relay_action.on[i] > -1)
      {
        return true;
      }
    }
  }
  return false;
}

bool um_am_automation_has_matrix(um_am_main_t *config)
{
  bool ok = config->ext;
  if (ok)
  {
    for (int i = 0; i < 6; i++)
    {
      if (config->opts.matrix_action[i].rel > -1 && config->opts.matrix_action[i].val > -1)
      {
        return true;
      }
    }
  }
  return false;
}

void um_am_automation_run(um_am_main_t *config)
{
  if (!config->ext)
    return;
  bool conditionMatch = false;
  switch (config->trigger.cond)
  {
  case UM_AM_TRIG_EQUAL:
    conditionMatch = config->value == config->trigger.value;
    config->inverted = !conditionMatch;
    break;
  case UM_AM_TRIG_MORE:
    conditionMatch = config->value > config->trigger.value;
    config->inverted = !conditionMatch;
    break;
  case UM_AM_TRIG_LESS:
    conditionMatch = config->value < config->trigger.value;
    config->inverted = !conditionMatch;
    break;
  default:
    // ????проблема, не получится инвертровать состояние в событии
    conditionMatch = true;
    break;
  }

  // Климат
  if (um_am_automation_has_boiler(config))
  {
    esp_event_post(APP_EVENTS, config->opts.boiler_action.ch == 1 ? EV_OT_CH_ON : EV_OT_CH_OFF, (void *)NULL, sizeof(NULL), portMAX_DELAY);
  }

  // Реле
  if (um_am_automation_has_relays(config))
  {
    for (int i = 0; i < sizeof(config->opts.relay_action.on) / sizeof(config->opts.relay_action.on[0]); i++)
    {
      if (config->opts.relay_action.on[i] > -1)
      {
        unsigned short int state;
        int prev_level;
        int real_channel;
        state = !config->inverted ? DO_HIGH : DO_LOW;
        // real_channel = do_map_channel(config->opts.relay_action.on[i]);
        real_channel = (int)config->opts.relay_action.on[i];

        prev_level = do_get_level(real_channel);

        if (prev_level != state)
        {
          // printf("\r\nOK %d, ext %d, inv %d, st %d\r\n", real_channel, config->ext, config->inverted, state);
          // return;
          do_set_level(real_channel, state); // state NOT invert, case ON[]
          ESP_LOGW("automations", "\r\n[NORMAL] Trigger: %d, value: %0.1f, inv:%d", (char)config->trigger.cond, config->trigger.value, config->inverted);
          for (int i = 0; i < 6; i++)
          {
            if (config->opts.relay_action.on[i] != -1)
            {
              ESP_LOGW("FIRE_AUTOMATION", "[ON] Toggle relay i:%d state %s", i, state ? "ON" : "OFF");
            }
          }
        }
      }
    }
    for (int i = 0; i < sizeof(config->opts.relay_action.off) / sizeof(config->opts.relay_action.off[0]); i++)
    {
      if (config->opts.relay_action.off[i] > -1)
      {
        unsigned short int state;
        int prev_level;
        int real_channel;
        state = !config->inverted ? DO_HIGH : DO_LOW;
        // real_channel = do_map_channel(config->opts.relay_action.off[i]);
        real_channel = config->opts.relay_action.off[i];
        prev_level = do_get_level(real_channel);
        if (prev_level != state)
        {
          do_set_level(real_channel, state);
          ESP_LOGW("automations", "\r\n[NORMAL] Trigger: %d, value: %0.1f, inv:%d", (char)config->trigger.cond, config->trigger.value, config->inverted);
          for (int i = 0; i < 6; i++)
          {
            if (config->opts.relay_action.on[i] != -1)
            {
              ESP_LOGW("FIRE_AUTOMATION", "[OFF] Toggle relay i:%d state %s", i, state ? "ON" : "OFF");
            }
          }
        }
      }
    }
  }
  if (um_am_automation_has_matrix(config))
  {
    int count = 6;
    um_am_matrix_t matrix;
    for (int i = 0; i < count; i++)
    {
      matrix = config->opts.matrix_action[i];
      if (matrix.rel > -1 && matrix.val > -1 && (int)config->value == (int)matrix.val)
      {
        int state = (do_port_index_t)matrix.state;
        bool inverse = matrix.inv;

        if (inverse)
        {
          state = do_get_level(matrix.rel) == DO_HIGH ? DO_LOW : DO_HIGH;
        }
        if (do_get_level(matrix.rel) != state)
        {
          do_set_level(matrix.rel, state);
        }
      }
    }
  }
}