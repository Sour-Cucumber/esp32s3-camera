#ifndef __TCA9554_H__
#define __TCA9554_H__

#include "esp_bit_defs.h"

typedef enum {
    TCA9554_GPIO_NUM_0 = BIT(0),
    TCA9554_GPIO_NUM_1 = BIT(1),
    TCA9554_GPIO_NUM_2 = BIT(2),
    TCA9554_GPIO_NUM_3 = BIT(3),
    TCA9554_GPIO_NUM_4 = BIT(4),
    TCA9554_GPIO_NUM_5 = BIT(5),
    TCA9554_GPIO_NUM_6 = BIT(6),
    TCA9554_GPIO_NUM_7 = BIT(7),
    TCA9554_GPIO_NUM_MAX
} esp_tca9554_gpio_num_t;

typedef enum {
    TCA9554_IO_LOW,
    TCA9554_IO_HIGH,
    TCA9554_LEVEL_ERROR
} esp_tca9554_io_level_t;

typedef enum {
    TCA9554_IO_OUTPUT,
    TCA9554_IO_INPUT,
    TCA9554_IO_CONFIG_ERROR
} esp_tca9554_io_config_t;

/**
 * @brief 初始化 TCA9554 扩展器
 * 
 * @return esp_err_t ESP_OK 成功，其他错误码失败
 */
esp_err_t tca9554_init(void);

/**
 * @brief 获取 TCA9554 扩展器输入引脚电平
 * 
 * @param gpio_num 引脚编号
 * @return esp_tca9554_io_level_t 电平  TCA9554_IO_LOW or TCA9554_IO_HIGH
 */
esp_tca9554_io_level_t tca9554_get_input_level(esp_tca9554_gpio_num_t gpio_num);

/**
 * @brief 获取 TCA9554 扩展器输出引脚电平
 * 
 * @param gpio_num 引脚编号
 * @return esp_tca9554_io_level_t 电平  TCA9554_IO_LOW or TCA9554_IO_HIGH
 */
esp_tca9554_io_level_t tca9554_get_output_level(esp_tca9554_gpio_num_t gpio_num);

/**
 * @brief 设置 TCA9554 扩展器输出引脚电平
 * 
 * @param gpio_num 引脚编号
 * @param level 电平  TCA9554_IO_LOW or TCA9554_IO_HIGH
 * @return esp_err_t ESP_OK 成功，其他错误码失败
 */
esp_err_t tca9554_set_output_level(esp_tca9554_gpio_num_t gpio_num, esp_tca9554_io_level_t level);

/**
 * @brief 获取 TCA9554 扩展器引脚配置   0: 输出，1: 输入
 * 
 * @param gpio_num 引脚编号
 * @return esp_tca9554_io_config_t  TCA9554_IO_OUTPUT or TCA9554_IO_INPUT
 */
esp_tca9554_io_config_t tca9554_get_io_config(esp_tca9554_gpio_num_t gpio_num);

/**
 * @brief 设置 TCA9554 扩展器引脚配置   0: 输出，1: 输入
 * 
 * @param gpio_num 引脚编号
 * @param config  TCA9554_IO_OUTPUT or TCA9554_IO_INPUT
 * @return esp_err_t ESP_OK 成功，其他错误码失败
 */
esp_err_t tca9554_set_io_config(esp_tca9554_gpio_num_t gpio_num, esp_tca9554_io_config_t config);

#endif
