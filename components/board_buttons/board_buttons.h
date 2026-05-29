#pragma once

#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Korvo2 板载 6 个 ADC 按键的编号
 */
typedef enum {
    BOARD_BTN_REC      = 0,  /**< REC 按键 */
    BOARD_BTN_MUTE     = 1,  /**< MUTE 按键 */
    BOARD_BTN_PLAY     = 2,  /**< PLAY 按键 */
    BOARD_BTN_SET      = 3,  /**< SET 按键 */
    BOARD_BTN_VOL_DOWN = 4,  /**< VOL- 按键 */
    BOARD_BTN_VOL_UP   = 5,  /**< VOL+ 按键 */
    BOARD_BTN_MAX,
} board_button_id_t;

/** 按键名称列表，索引对应 board_button_id_t */
extern const char *btn_names[BOARD_BTN_MAX];

/**
 * @brief 初始化板载 6 个 ADC 按键
 *
 * 按键原理：6 个按键共享一根 ADC 通道(ADC_UNIT_1, CH4)，
 * 通过分压电阻产生不同电压值来区分按键。
 *
 * @note 需要在 main/CMakeLists.txt 的 REQUIRES 中添加 board_buttons
 * @note 需要 menuconfig 启用 ADC driver (CONFIG_ADC_ONESHOT_CTRL=y)
 *
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t board_buttons_init(void);

/**
 * @brief 获取指定按键的句柄
 *
 * @param[in] id 按键编号
 * @return button_handle_t 按键句柄，失败返回 NULL
 */
button_handle_t board_button_get_handle(board_button_id_t id);

#ifdef __cplusplus
}
#endif
