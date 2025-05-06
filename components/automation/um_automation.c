
#include "um_automation.h"
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
      }
    ]
  }
 */

void um_am_test()
{
}

/**
 * Fill automation config by sensor json value
 * IMPORTANT!!! cJSON_Delete must called in parent method
 *
 * @return  void
 */
void um_am_parse_json_config(cJSON *sensor_json, um_am_main_t *config)
{
  for (int j = 0; j < 6; j++)
  {
    config->opts.relay_action.off[j] = -1;
    config->opts.relay_action.on[j] = -1;
  }
  config->opts.boiler_action.ch = -1;
  // json foreach
  const cJSON *ext = cJSON_GetObjectItem(sensor_json, "ext");
  if (ext != NULL)
  {
    config->ext = cJSON_IsTrue(ext);
    // Parse trigger
    cJSON *trigger = cJSON_GetObjectItem(sensor_json, "trigger");
    if (trigger != NULL)
    {
      /** ============= TRIGGER ========================== **/
      cJSON *cond = cJSON_GetObjectItem(trigger, "cond");
      cJSON *value = cJSON_GetObjectItem(trigger, "value");
      config->trigger.cond = (um_am_trigger_type_t)cond->valuestring;
      config->trigger.value = value->valuedouble;
      /** ============= TRIGGER ========================== **/

      // If trigger ok, parse actions
      cJSON *actions = cJSON_GetObjectItem(sensor_json, "actions");
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
                config->opts.relay_action.on[i] = on->valueint;
                i++;
              }
            }

            i = 0;
            relay_index = NULL;

            if (off != NULL && cJSON_IsArray(off))
            {
              cJSON_ArrayForEach(relay_index, off)
              {
                config->opts.relay_action.off[i] = off->valueint;
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
          /** ============= BOILER ACTIONS ========================== **/
          default:
            break;
          }
        }
      }
    }
  }
}