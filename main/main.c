#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tca9554.h"
#include "lcd_board.h"

#define LED_RED_NUM                 TCA9554_GPIO_NUM_6     // 红色 LED 对应tca9554引脚6
#define LED_BLUE_NUM                TCA9554_GPIO_NUM_7     // 蓝色 LED 对应tca9554引脚7

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

static esp_lcd_panel_handle_t lcd_panel;

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

static void lcd_test(void *param)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    uint16_t colors[] = { 0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000 };
    int color_count = sizeof(colors) / sizeof(colors[0]);
    int idx = 0;

    uint16_t *buf = heap_caps_malloc(LCD_WIDTH * 60 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE("lcd_test", "Failed to alloc buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        // 提前把颜色值转成大端序 (ILI9341 期望高字节在前)
        uint16_t color_le = colors[idx];
        uint8_t *c = (uint8_t *)&color_le;
        uint16_t color_be = ((uint16_t)c[0] << 8) | c[1];

        for (int i = 0; i < LCD_WIDTH * 60; i++) {
            buf[i] = color_be;
        }
        for (int y = 0; y < LCD_HEIGHT; y += 60) {
            esp_lcd_panel_draw_bitmap(lcd_panel, 0, y, LCD_WIDTH, y + 60, buf);
        }
        idx = (idx + 1) % color_count;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {

    ESP_LOGI("main", "TCA9554 driver initialized");
    tca9554_init();

    // 1. 配置引脚方向：将所有引脚设为输出 (向寄存器 0x03 写入 0x00)
    ESP_LOGI("main", "Setting led GPIOs to output");
    tca9554_set_io_config(LED_RED_NUM, TCA9554_IO_OUTPUT);
    tca9554_set_io_config(LED_BLUE_NUM, TCA9554_IO_OUTPUT);
    ESP_LOGI("main", "Setting led GPIOs to light");
    tca9554_set_output_level(LED_RED_NUM, TCA9554_IO_HIGH);
    tca9554_set_output_level(LED_BLUE_NUM, TCA9554_IO_HIGH);
    vTaskDelay(1000);

    ESP_LOGI("main", "Setting led GPIOs to dark");
    tca9554_set_output_level(LED_RED_NUM, TCA9554_IO_LOW);
    tca9554_set_output_level(LED_BLUE_NUM, TCA9554_IO_LOW);
    vTaskDelay(1000);

    ESP_LOGI("TASK", "redled_task created");
    xTaskCreatePinnedToCore(red_led, "redled_task", 2048, NULL, 1, NULL, 0);
    ESP_LOGI("TASK", "blueled_task created");
    xTaskCreatePinnedToCore(blue_led, "blueled_task", 2048, NULL, 1, NULL, 0);

    ESP_LOGI("main", "Initializing LCD");
    ESP_ERROR_CHECK(lcd_board_init(&lcd_panel));

    ESP_LOGI("TASK", "lcd_test_task created");
    xTaskCreatePinnedToCore(lcd_test, "lcd_test_task", 4096, NULL, 2, NULL, 0);
}
