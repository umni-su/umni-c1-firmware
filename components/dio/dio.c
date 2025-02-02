#include "dio.h"
#include "esp_event.h"

#include "../../main/includes/events.h"
#include "../nvs/nvs.h"

uint8_t input_data = 0xff;

uint8_t output_data = 0xff;

static i2c_dev_t pcf8574_output_dev_t;

static i2c_dev_t pcf8574_input_dev_t;

bool need_blink_stat = false;

bool need_blink_err = false;

esp_err_t do_register_events()
{
    return ESP_OK;
    // return esp_event_handler_register(APP_EVENTS, S_CONFIG_SPIFFS_READY, &config_spiffs_mounted, NULL);
}

uint8_t di_get_nvs_state()
{
    int8_t state = um_nvs_read_i8(NVS_KEY_RELAYS);
    if ((int)state == -1)
    {
        state = 0xff;
    }
    return (uint8_t)state;
}

esp_err_t di_set_nvs_state()
{
    return um_nvs_write_i8(NVS_KEY_RELAYS, output_data);
}

esp_err_t init_do()
{
    esp_err_t res = ESP_OK;
    output_data = di_get_nvs_state();
    memset(&pcf8574_output_dev_t, 0, sizeof(i2c_dev_t));
    pcf8574_output_dev_t.cfg.master.clk_speed = 5000; // Hz
    res = pcf8574_init_desc(&pcf8574_output_dev_t, I2C_DO_ADDR, 0, CONFIG_UMNI_I2C_SDA, CONFIG_UMNI_I2C_SCL);
    if (res == ESP_OK)
    {
        res = pcf8574_port_write(&pcf8574_output_dev_t, output_data);
        esp_event_post(APP_EVENTS, EV_DO_INIT, NULL, sizeof(NULL), portMAX_DELAY);
    }
    else
    {
        ESP_LOGE("dio", "%s", esp_err_to_name(res));
    }

    return res;
}

do_level_t do_get_level(do_port_index_t channel)
{
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
        res = di_set_nvs_state();
        ESP_LOGW("DO STATE", "%u", di_get_nvs_state());
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
    led_blink_t *led_blink_conf = (led_blink_t *)arg;
    int timeout = led_blink_conf->timeout;
    int count = (int)led_blink_conf->count;
    int pause = (int)led_blink_conf->pause;
    di_port_index_t chan = (int)led_blink_conf->chan;
    ESP_LOGI("do_blink_led_stat_start_task", "Timeout: %d, pause: %d, count: %d", timeout, pause, count);

    while (need_blink_stat)
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
    vTaskDelete(NULL);
}

/// @brief Индикация загрузки при включении
void do_blink_led_stat_start_at_boot()
{
    do_blink_led_stop(LED_STAT);
    need_blink_stat = true;
    led_blink_t led_blink_conf = {
        .chan = LED_STAT,
        .count = 2,
        .pause = 0,
        .timeout = 500};
    xTaskCreate(do_blink_led_start_task, "do_blink_led_stat_start_at_boot", configMINIMAL_STACK_SIZE * 6, &led_blink_conf, 20, NULL);
}

/// @brief Индикация нормального режима работы
void do_blink_led_stat_start_working()
{
    do_blink_led_stop(LED_STAT);
    need_blink_stat = true;
    led_blink_t led_blink_conf = {
        .chan = LED_STAT,
        .count = 1,
        .pause = 10000,
        .timeout = 100};
    xTaskCreate(do_blink_led_start_task, "do_blink_led_stat_start_working", configMINIMAL_STACK_SIZE * 6, &led_blink_conf, 20, NULL);
}

/// @brief Индикация при прошивке
void do_blink_led_stat_start_flashing()
{
    do_blink_led_stop(LED_STAT);
    need_blink_stat = true;
    led_blink_t led_blink_conf = {
        .chan = LED_STAT,
        .count = 1,
        .pause = 0,
        .timeout = 200};
    xTaskCreate(do_blink_led_start_task, "do_blink_led_stat_start_flashing", configMINIMAL_STACK_SIZE * 6, &led_blink_conf, 20, NULL);
}

/// @brief Индикация ошибки
void do_blink_led_error()
{
    do_blink_led_stop(LED_ERR);
    need_blink_err = true;
    led_blink_t led_blink_conf = {
        .chan = LED_ERR,
        .count = 4,
        .pause = 0,
        .timeout = 1500};
    xTaskCreate(do_blink_led_start_task, "do_blink_led_error", configMINIMAL_STACK_SIZE * 6, &led_blink_conf, 20, NULL);
}

/// @brief Остановка индикации
/// @param channel
void do_blink_led_stop(do_port_index_t channel)
{
    switch (channel)
    {
    case LED_STAT:
        need_blink_stat = false;
        break;
    case LED_ERR:
        need_blink_err = false;
        break;
    default:
        break;
    }
}

void do_blink_led_err_start(int timeout)
{
    need_blink_err = true;
}

void do_blink_led_err_stop()
{
    need_blink_err = false;
}

void do_blink_led_err_start_task(void *arg)
{
}

static void IRAM_ATTR catch_di_interrupts(void *args)
{
    xTaskCreate(di_interrupt_task, "di_interrupt_task", 4096, NULL, 20, NULL);
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
    }

    return res;
}

void di_interrupt_task(void *arg)
{
    uint8_t current_state;
    pcf8574_port_read(&pcf8574_input_dev_t, &current_state);
    for (int i = 0; i < 8; i++)
    {
        int pin_level = current_state >> i & 0x01;
        int current_pin__level = input_data >> i & 0x01;
        if (pin_level != current_pin__level)
        {
            ESP_LOGW("dio intr", "Pin #%d has value %d", i, pin_level);
            input_data = current_state; // set last level to input pins
        }
        // else
        // {

        //}
    }
    vTaskDelete(NULL);
}