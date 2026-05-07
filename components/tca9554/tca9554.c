/**
 * @brief TCA9554驱动
 * 
 */
#include <stdio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include "tca9554.h"

#define TCA9554_INPUT_PORT              0x00
#define TCA9554_OUTPUT_PORT             0x01
#define TCA9554_POLARITY_INVERSION_PORT 0x02
#define TCA9554_CONFIGURATION_PORT      0x03

#define I2C_MASTER_SDA_IO           17    // SDA引脚
#define I2C_MASTER_SCL_IO           18    // SCL引脚
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define TCA9554_ADDR                0x20  // TCA9554地址

#define SET_BITS(_m, _s, _v)  ((_v) ? (_m)|((_s)) : (_m)&~((_s))) // mask, selector, value
#define GET_BITS(_m, _s)      (((_m) & (_s)) ? true : false)

static const char *TAG = "TCA9554";

// 初始化 TCA9554的I2C总线
static esp_err_t i2c_bus_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C master");
        return err;
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver");
        return err;
    }
    return ESP_OK;
}

// 向 TCA9554的寄存器写入数据
static esp_err_t tca9554_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, TCA9554_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}
// 从 TCA9554的寄存器读取数据
static esp_err_t tca9554_read_reg(uint8_t reg_addr, uint8_t *data) {
    //  发送所读取的寄存器地址
    i2c_master_write_to_device(I2C_MASTER_NUM, TCA9554_ADDR, &reg_addr, sizeof(reg_addr), pdMS_TO_TICKS(100));
    //  读取数据寄存器内容
    return i2c_master_read_from_device(I2C_MASTER_NUM, TCA9554_ADDR, data, 1,pdMS_TO_TICKS(100));
}

esp_tca9554_io_level_t tca9554_get_input_level(esp_tca9554_gpio_num_t gpio_num) {
    uint8_t data;
    if(gpio_num < TCA9554_GPIO_NUM_MAX) {
        esp_err_t err = tca9554_read_reg(TCA9554_INPUT_PORT, &data);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read input level");
            return TCA9554_LEVEL_ERROR;
        }
        return GET_BITS(data, gpio_num);
    }
    ESP_LOGE(TAG, "Invalid GPIO number %d", gpio_num);
    return TCA9554_LEVEL_ERROR;
}

esp_tca9554_io_level_t tca9554_get_output_level(esp_tca9554_gpio_num_t gpio_num) {
    uint8_t data;
    if(gpio_num < TCA9554_GPIO_NUM_MAX) {
        esp_err_t err = tca9554_read_reg(TCA9554_OUTPUT_PORT, &data);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output level");
            return TCA9554_LEVEL_ERROR;
        }
        return GET_BITS(data, gpio_num);
    }
    ESP_LOGE(TAG, "Invalid GPIO number %d", gpio_num);
    return TCA9554_LEVEL_ERROR; 
}

esp_err_t tca9554_set_output_level(esp_tca9554_gpio_num_t gpio_num, esp_tca9554_io_level_t level) {
    uint8_t data;
    esp_err_t err = ESP_OK;
    if(gpio_num < TCA9554_GPIO_NUM_MAX) {
        err = tca9554_read_reg(TCA9554_OUTPUT_PORT, &data);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output level");
            return err;
        }
        data = SET_BITS(data, gpio_num, level);
        err = tca9554_write_reg(TCA9554_OUTPUT_PORT, data);
    }
    else {
        ESP_LOGE(TAG, "Invalid GPIO number %d", gpio_num);
    }
    return err;
}

esp_tca9554_io_config_t tca9554_get_io_config(esp_tca9554_gpio_num_t gpio_num) {
    uint8_t data;
    if(gpio_num < TCA9554_GPIO_NUM_MAX) {
        esp_err_t err = tca9554_read_reg(TCA9554_CONFIGURATION_PORT, &data);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port");
            return TCA9554_IO_CONFIG_ERROR;
        }
        return GET_BITS(data, gpio_num);
    }
    ESP_LOGE(TAG, "Invalid GPIO number %d", gpio_num);
    return TCA9554_IO_CONFIG_ERROR;
}

esp_err_t tca9554_set_io_config(esp_tca9554_gpio_num_t gpio_num, esp_tca9554_io_config_t config) {
    uint8_t data;
    esp_err_t err = ESP_OK;
    if(gpio_num < TCA9554_GPIO_NUM_MAX) {
        err = tca9554_read_reg(TCA9554_CONFIGURATION_PORT, &data);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port");
            return err;
        }
        data = SET_BITS(data, gpio_num, config);
        err = tca9554_write_reg(TCA9554_CONFIGURATION_PORT, data);
    }
    else {
        ESP_LOGE(TAG, "Invalid GPIO number %d", gpio_num);
    }
    return err;
}

esp_err_t tca9554_init() {
    esp_err_t err = i2c_bus_init();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return err;
    }
    return ESP_OK;
}
