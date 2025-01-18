#include "dio.h"

uint8_t input_data = 0xff;

uint8_t output_data = 0x00;

static i2c_dev_t pcf8574_outputs;

static i2c_dev_t pcf8574_inputs;

esp_err_t init_do()
{
    esp_err_t res = ESP_OK;
    memset(&pcf8574_outputs, 0, sizeof(i2c_dev_t));
    pcf8574_outputs.cfg.master.clk_speed = 5000; // Hz
    res = pcf8574_init_desc(&pcf8574_outputs, I2C_DO_ADDR, 0, CONFIG_UMNI_I2C_SDA, CONFIG_UMNI_I2C_SCL);
    if (res == ESP_OK)
    {
        res = pcf8574_port_write(&pcf8574_outputs, output_data);
    }
    else
    {
        ESP_LOGE("dio", "%s", esp_err_to_name(res));
    }

    return res;
}

do_level_t do_get_level(do_port_index_t channel)
{
    esp_err_t res = pcf8574_port_read(&pcf8574_outputs, &output_data);
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
    return pcf8574_port_write(&pcf8574_outputs, output_data);
}

esp_err_t init_di()
{

    return ESP_OK;
}