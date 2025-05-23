#include "pcf8574.h"
#include "dio.h"
#include "esp_event.h"

#include "../../main/includes/events.h"
#include "../nvs/nvs.h"
#include "../config/config.h"
#include "../automation/automation.h"

static uint8_t input_data = 0xff;

static uint8_t output_data = 0xff;

static i2c_dev_t pcf8574_output_dev_t;

static i2c_dev_t pcf8574_input_dev_t;

static bool need_blink_stat = false;

static bool need_blink_err = false;

static led_blink_t led_blink_stat_conf;

static led_blink_t led_blink_err_conf;

static uint8_t current_state;

/** DI Automation action configuration */
static um_am_main_t di_automation[6];

static bool dio_initialized = false;

uint8_t do_get_nvs_state()
{
    int8_t state = um_nvs_read_i8(NVS_KEY_RELAYS);
    if ((int)state == -1)
    {
        state = 0xff;
    }
    return (uint8_t)state;
}

esp_err_t do_set_nvs_state()
{
    return um_nvs_write_i8(NVS_KEY_RELAYS, output_data);
}

esp_err_t do_set_all_ff()
{
    return pcf8574_port_write(&pcf8574_output_dev_t, 0xff);
}

esp_err_t do_restore_all_values()
{
    return pcf8574_port_write(&pcf8574_output_dev_t, output_data);
}

esp_err_t init_do()
{
    char *config_file = um_config_get_config_file_dio();

    cJSON *config = cJSON_Parse(config_file);
    if (cJSON_HasObjectItem(config, "di"))
    {
        cJSON *di_ar = cJSON_GetObjectItem(config, "di");
        cJSON *di_el = NULL;

        cJSON_ArrayForEach(di_el, di_ar)
        {
            int port = cJSON_HasObjectItem(di_el, "index") ? cJSON_GetObjectItem(di_el, "index")->valueint : -1;
            if (port >= 0)
            {
                // TEST AUTOMATIONS HERE
                um_am_parse_json_config(di_el, &di_automation[port]);
                // TEST AUTOMATIONS HERE
            }
        }
    }

    cJSON_Delete(config);
    free((void *)config_file);

    esp_err_t res = ESP_OK;
    output_data = do_get_nvs_state();
    memset(&pcf8574_output_dev_t, 0, sizeof(i2c_dev_t));
    pcf8574_output_dev_t.cfg.master.clk_speed = 5000; // Hz
    res = pcf8574_init_desc(&pcf8574_output_dev_t, I2C_DO_ADDR, 0, CONFIG_UMNI_I2C_SDA, CONFIG_UMNI_I2C_SCL);
    if (res == ESP_OK)
    {
        res = pcf8574_port_write(&pcf8574_output_dev_t, output_data);
        do_set_nvs_state();
        // Start boot blinking
        do_blink_led_stat_start_at_boot();
        // Init led err
        do_blink_led_err_init();

        // esp_log_set_default_level();

        esp_event_post(APP_EVENTS, EV_DO_INIT, NULL, sizeof(NULL), portMAX_DELAY);

        dio_initialized = true;
    }
    else
    {
        ESP_LOGE("dio", "%s", esp_err_to_name(res));
    }

    return res;
}

do_port_index_t do_map_channel(int channel)
{
    switch (channel)
    {
    case 0:
        return DO_1;
    case 1:
        return DO_2;
    case 2:
        return DO_3;
    case 3:
        return DO_4;
    case 4:
        return DO_5;
    case 5:
        return DO_6;
    case 6:
        return LED_STAT;
    case 7:
        return LED_ERR;

    default:
        return 0;
    }
}

do_port_index_t di_map_channel(int channel)
{
    switch (channel)
    {
    case 0:
        return DI_1;
    case 1:
        return DI_2;
    case 2:
        return DI_3;
    case 3:
        return DI_4;
    case 4:
        return DI_5;
    case 5:
        return DI_6;
    case 6:
        return DI_FN;
    case 7:
        return DI_CFG;

    default:
        return 0;
    }
}

do_level_t do_get_level(do_port_index_t channel)
{
    if (!dio_initialized)
        return 0;
    // channel = do_map_channel(channel);
    esp_err_t res = pcf8574_port_read(&pcf8574_output_dev_t, &output_data);
    if (res == ESP_OK)
    {
        // Так как 0 на ножке pcf8574 инвертируется через логический конвертер в 1, тут применяется инверсия результата
        return ((output_data >> channel) & 0x01) == 0 ? DO_HIGH : DO_LOW;
    }
    else
    {
        // todo error blinking
    }
    return 0;
}

esp_err_t do_set_level(do_port_index_t channel, do_level_t level)
{
    if (!dio_initialized)
        return ESP_FAIL;
    // channel = do_map_channel(channel);
    if (level == DO_LOW)
    {
        output_data = output_data | (1 << channel);
    }
    else
    {
        output_data = output_data & ~(1 << channel);
    }
    // output_data = output_data >> channel & level;
    esp_err_t res = pcf8574_port_write(&pcf8574_output_dev_t, output_data);
    if (res == ESP_OK)
    {
        res = do_set_nvs_state();
        um_ev_message_dio message = {
            .index = channel,
            .level = level};

        if (channel != CONFIG_UMNI_ERR_LED && channel != CONFIG_UMNI_STAT_LED)
        {
            esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_DO, (void *)&message, sizeof(message), portMAX_DELAY);
            // ESP_LOGW("DO STATE", "%u", do_get_nvs_state());
        }
    }
    if (res != ESP_OK)
    {
        ESP_LOGE("dio", "do_set_level error %s", esp_err_to_name(res));
    }
    return res;
}

/// @brief Задача обработки мигания системных светодиодов
/// @param arg
void do_blink_led_start_task(void *arg)
{
    while (true)
    {
        led_blink_t *led_blink_conf = (led_blink_t *)arg;
        int timeout = led_blink_conf->timeout;
        int count = (int)led_blink_conf->count;
        int pause = (int)led_blink_conf->pause;
        bool active = (bool)led_blink_conf->active;
        di_port_index_t chan = (int)led_blink_conf->chan;
        // ESP_LOGI("do_blink_led_stat_start_task", "[CHAN %d]Timeout: %d, pause: %d, count: %d", chan, timeout, pause, count);
        if (active)
        {
            for (int i = 0; i < count; i++)
            {
                do_set_level(chan, DO_HIGH);
                vTaskDelay(timeout / portTICK_PERIOD_MS);
                do_set_level(chan, DO_LOW);
                vTaskDelay(timeout / portTICK_PERIOD_MS);
            }
            vTaskDelay(pause / portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

void do_blink_led_err_init()
{
    do_blink_led_err_stop();
    need_blink_stat = true;
    led_blink_err_conf.active = false;
    led_blink_err_conf.chan = LED_ERR;
    led_blink_err_conf.count = 2;
    led_blink_err_conf.pause = 0;
    led_blink_err_conf.timeout = 1000;
    xTaskCreate(do_blink_led_start_task, "do_blink_led_err", configMINIMAL_STACK_SIZE * 2, &led_blink_err_conf, 1, NULL);
}

/// @brief Индикация загрузки при включении
void do_blink_led_stat_start_at_boot()
{
    do_blink_led_stop(LED_STAT);
    need_blink_stat = true;
    led_blink_stat_conf.active = true;
    led_blink_stat_conf.chan = LED_STAT;
    led_blink_stat_conf.count = 2;
    led_blink_stat_conf.pause = 0;
    led_blink_stat_conf.timeout = 1000;
    xTaskCreate(do_blink_led_start_task, "do_blink_led_stat", configMINIMAL_STACK_SIZE * 2, &led_blink_stat_conf, 1, NULL);
}

/// @brief Индикация нормального режима работы
void do_blink_led_stat_start_working()
{
    do_blink_led_stop(LED_ERR);
    do_blink_led_stop(LED_STAT);
    need_blink_stat = true;
    led_blink_stat_conf.active = true;
    led_blink_stat_conf.chan = LED_STAT;
    led_blink_stat_conf.count = 1;
    led_blink_stat_conf.pause = 10000;
    led_blink_stat_conf.timeout = 100;
}

/// @brief Индикация при прошивке
void do_blink_led_stat_start_flashing()
{
    do_blink_led_stop(LED_STAT);
    do_blink_led_stop(LED_ERR);
    need_blink_stat = true;
    led_blink_stat_conf.active = true;
    led_blink_stat_conf.chan = LED_STAT;
    led_blink_stat_conf.count = 1;
    led_blink_stat_conf.pause = 0;
    led_blink_stat_conf.timeout = 150;
}

/// @brief Индикация ошибки
void do_blink_led_error()
{
    do_blink_led_stop(LED_ERR);
    need_blink_err = true;
    led_blink_err_conf.active = true;
    led_blink_err_conf.chan = LED_ERR;
    led_blink_err_conf.count = 4;
    led_blink_err_conf.pause = 0;
    led_blink_err_conf.timeout = 1500;
}
/// @brief Остановка индикации
/// @param channel
void do_blink_led_stop(do_port_index_t channel)
{
    switch (channel)
    {
    case LED_STAT:
        led_blink_stat_conf.active = false;
        break;
    case LED_ERR:
        led_blink_err_conf.active = false;
        break;
    default:
        break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void do_blink_led_err_start(int timeout)
{
    led_blink_err_conf.active = true;
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void do_blink_led_err_stop()
{
    led_blink_err_conf.active = false;
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void do_blink_led_stat_stop()
{
    led_blink_stat_conf.active = false;
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void do_blink_led_err_start_task(void *arg)
{
}

static void IRAM_ATTR catch_di_interrupts(void *args)
{
    xTaskCreate(di_interrupt_task, "di_interrupt_task", 4096, NULL, 2, NULL);
}

esp_err_t init_di()
{
    esp_err_t res = ESP_OK;
    gpio_set_direction(CONFIG_UMNI_DI_INT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_UMNI_DI_INT_PIN, GPIO_FLOATING);
    gpio_isr_handler_add(CONFIG_UMNI_DI_INT_PIN, catch_di_interrupts, NULL);
    gpio_set_intr_type(CONFIG_UMNI_DI_INT_PIN, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(CONFIG_UMNI_DI_INT_PIN);

    memset(&pcf8574_input_dev_t, 0, sizeof(i2c_dev_t));
    pcf8574_input_dev_t.cfg.master.clk_speed = 5000; // Hz
    res = pcf8574_init_desc(&pcf8574_input_dev_t, I2C_DI_ADDR, 0, CONFIG_UMNI_I2C_SDA, CONFIG_UMNI_I2C_SCL);
    if (res == ESP_OK)
    {
        pcf8574_port_write(&pcf8574_input_dev_t, (uint8_t)input_data);
        vTaskDelay(20 / portTICK_PERIOD_MS);
        pcf8574_port_read(&pcf8574_input_dev_t, &input_data);

        if (di_is_config_mode())
        {
            ESP_LOGI("dio", "UMNI in config mode");
        }
        else
        {
            ESP_LOGI("dio", "UMNI in normal mode. Turn off log level programaticaly");
            esp_log_level_set(NULL, ESP_LOG_NONE);
        }
    }

    return res;
}

di_level_t di_get_level(di_port_index_t index)
{
    return input_data >> index & 0x01;
}

bool di_is_config_mode()
{
    di_level_t level = di_get_level(DI_CFG);
    return level == DI_LOW;
}

/**
 * Main interrupt handler.a64lCatch interrupts from pcf8574
 *
 * @param   void  arg
 *
 * @return  void
 */
void di_interrupt_task(void *arg)
{
    pcf8574_port_read(&pcf8574_input_dev_t, &current_state);
    for (int i = 0; i < 8; i++)
    {
        int pin_level = current_state >> i & 0x01;
        int current_pin_level = input_data >> i & 0x01;
        if (i != DI_FN && i != DI_CFG)
        {
            if (pin_level != current_pin_level)
            {
                ESP_LOGW("dio intr", "Pin #%d has value %d", i, pin_level);
                input_data = current_state; // set last level to input pins

                // Update automation value
                di_automation[i].value = pin_level;
                di_automation[i].inverted = pin_level == DI_LOW;
                // Run automation
                um_am_automation_run(&di_automation[i]);

                um_ev_message_dio message = {
                    .index = i,
                    .level = pin_level};
                esp_event_post(APP_EVENTS, EV_STATUS_CHANGED_DI, &message, sizeof(message), portMAX_DELAY);
            }
        }
        else
        {
            if (pin_level != current_pin_level)
            {
                input_data = current_state;
                if (i == DI_FN)
                {
                    ESP_LOGI("DI_FN", "Function button is pressed! Level is: %d, prev level: %d", pin_level, current_pin_level);
                }
                if (i == DI_CFG)
                {
                    ESP_LOGI("DI_CFG", "Config switch pressed! Level is: %d, prev level: %d", pin_level, current_pin_level);
                }
                else
                {
                    ESP_LOGI("DI_?????", "Pressed! Level is: %d, prev level: %d", pin_level, current_pin_level);
                }
            }
        }
    }
    vTaskDelete(NULL);
}

int8_t di_get_state()
{
    return input_data;
}