#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "tca9554.h"
#include "lcd_board.h"

#define LED_RED_NUM                 TCA9554_GPIO_NUM_6     // 红色 LED 对应tca9554引脚6
#define LED_BLUE_NUM                TCA9554_GPIO_NUM_7     // 蓝色 LED 对应tca9554引脚7

#define LCD_H_RES                   320
#define LCD_V_RES                   240

/* 全局 LCD 面板句柄 */
static esp_lcd_panel_handle_t s_lcd_panel = NULL;


void blue_led(void *param)
{
    while (1)
    {
        tca9554_set_output_level(LED_BLUE_NUM, TCA9554_IO_HIGH);
        vTaskDelay(200);
        tca9554_set_output_level(LED_BLUE_NUM, TCA9554_IO_LOW);
        vTaskDelay(200);
    }
}

void red_led(void *param)
{
    while (1)
    {
        tca9554_set_output_level(LED_RED_NUM, TCA9554_IO_HIGH);
        vTaskDelay(100);
        tca9554_set_output_level(LED_RED_NUM, TCA9554_IO_LOW);
        vTaskDelay(100);
    }
}



void app_main(void)
{
    /* ── 初始化 TCA9554 ── */
    ESP_LOGI("main", "Initializing TCA9554");
    ESP_ERROR_CHECK(tca9554_init());

    /* ── 初始化 LCD ── */
    ESP_LOGI("main", "Initializing LCD");
    ESP_ERROR_CHECK(lcd_board_init(&s_lcd_panel));

    /* ── LCD 填充测试 (红色) ── */
    uint16_t *fb = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (fb) {
        for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
            fb[i] = 0xF800; /* RGB565 红色 */
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, LCD_H_RES, LCD_V_RES, fb));
        free(fb);
        ESP_LOGI("main", "LCD filled with red");
    }

    /* ── 配置 LED 引脚 ── */
    ESP_LOGI("main", "Setting LED GPIOs to output");
    tca9554_set_io_config(LED_RED_NUM, TCA9554_IO_OUTPUT);
    tca9554_set_io_config(LED_BLUE_NUM, TCA9554_IO_OUTPUT);

    /* ── 创建 LED 闪烁任务 ── */
    ESP_LOGI("main", "Creating LED tasks");
    xTaskCreatePinnedToCore(red_led, "redled_task", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(blue_led, "blueled_task", 2048, NULL, 1, NULL, 0);
}