#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LCD 全链路：SPI 总线 → TCA9554 控制引脚 → ILI9341 面板 → 背光
 *
 * @note 调用此函数前需先初始化 TCA9554 (tca9554_init)
 *
 * @param[out] out_panel 返回 LCD 面板句柄，后续可用 esp_lcd_panel_* API 操作
 * @return
 *        - ESP_OK 成功
 *        - 其他 失败
 */
esp_err_t lcd_board_init(esp_lcd_panel_handle_t *out_panel);

/**
 * @brief 设置背光亮度
 *
 * @param percent 亮度百分比 0~100，0=灭，100=最亮
 * @return
 *        - ESP_OK 成功
 *        - 其他 失败
 */
esp_err_t lcd_board_set_backlight(uint8_t percent);

#ifdef __cplusplus
}
#endif
