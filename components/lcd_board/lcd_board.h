#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 ILI9341 LCD，返回面板句柄。
 *
 * 硬件引脚：
 *   MOSI=IO0, SCLK=IO1, DC=IO2 (物理 GPIO)
 *   RST=TCA9554 P2, BL=TCA9554 P1, CS=TCA9554 P3 (I2C GPIO 扩展)
 *
 * @param out_panel  输出 esp_lcd_panel_handle_t，后续所有绘图操作通过此句柄调用
 * @return ESP_OK 成功
 */
esp_err_t lcd_board_init(esp_lcd_panel_handle_t *out_panel);

/**
 * @brief 背光开关（TCA9554 P1）
 */
esp_err_t lcd_board_set_backlight(bool on);

/* ================================================================
 *  以下 API 均由 esp_lcd_panel_ops.h 提供，通过 lcd_board_init()
 *  返回的 panel 句柄调用。lcd_board.h 已包含该头文件，无需再 include。
 *
 *   绘图:   esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, data)
 *   旋转:   esp_lcd_panel_swap_xy(panel, swap)
 *   镜像:   esp_lcd_panel_mirror(panel, mirror_x, mirror_y)
 *   反色:   esp_lcd_panel_invert_color(panel, invert)
 *   开关显示: esp_lcd_panel_disp_on_off(panel, on_off)
 *   删除面板: esp_lcd_panel_del(panel)
 *   重置:    esp_lcd_panel_reset(panel)
 *   初始化:  esp_lcd_panel_init(panel)
 * ================================================================
 */

#ifdef __cplusplus
}
#endif
