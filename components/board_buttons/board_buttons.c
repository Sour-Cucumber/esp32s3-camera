#include "board_buttons.h"
#include "button_adc.h"
#include "esp_log.h"

static const char *TAG = "board_buttons";

const char *btn_names[BOARD_BTN_MAX] = {
    [BOARD_BTN_REC]      = "REC",
    [BOARD_BTN_MUTE]     = "MUTE",
    [BOARD_BTN_PLAY]     = "PLAY",
    [BOARD_BTN_SET]      = "SET",
    [BOARD_BTN_VOL_DOWN] = "VOL-",
    [BOARD_BTN_VOL_UP]   = "VOL+",
};

static button_handle_t s_btns[BOARD_BTN_MAX];

esp_err_t board_buttons_init(void)
{
    const button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };

    button_adc_config_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = 4,
    };

    /* ESP32-S3-Korvo2 6 个按键的分压值 (mV) */
    const uint16_t vol[BOARD_BTN_MAX] = {2410, 1980, 1570, 1180, 820, 380};

    for (size_t i = 0; i < BOARD_BTN_MAX; i++) {
        adc_cfg.button_index = i;

        /* 以分压值为中心，±100mV 作为电压窗口 */
        adc_cfg.min = (vol[i] > 100) ? (vol[i] - 100) : 0;
        adc_cfg.max = vol[i] + 100;

        esp_err_t ret = iot_button_new_adc_device(&btn_cfg, &adc_cfg, &s_btns[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create button %s: %d", btn_names[i], ret);
            return ret;
        }
        ESP_LOGI(TAG, "Button [%s] created, voltage range: %d-%d mV",
                 btn_names[i], adc_cfg.min, adc_cfg.max);
    }

    ESP_LOGI(TAG, "All %d board buttons initialized", BOARD_BTN_MAX);
    return ESP_OK;
}

button_handle_t board_button_get_handle(board_button_id_t id)
{
    if (id >= BOARD_BTN_MAX) {
        return NULL;
    }
    return s_btns[id];
}
