#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tca9554.h"
#include "lcd_board.h"
#include "camera_board.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_camera.h"
#include "sdio.h"

#define LED_RED_NUM                 TCA9554_GPIO_NUM_6     // 红色 LED 对应tca9554引脚6
#define LED_BLUE_NUM                TCA9554_GPIO_NUM_7     // 蓝色 LED 对应tca9554引脚7

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

static esp_lcd_panel_handle_t lcd_panel;

void app_main(void) {

    ESP_LOGI("main", "Initializing TCA9554");                                                                                                           
    ESP_ERROR_CHECK(tca9554_init());

    ESP_LOGI("main", "Initializing LCD");
    ESP_ERROR_CHECK(lcd_board_init(&lcd_panel));

    ESP_LOGI("main", "Initializing Camera");                                                                                                     
    ESP_ERROR_CHECK(camera_init(RGB565));   

    ESP_LOGI("main", "Initializing SD Card");
    sdcard_init();

    uint8_t camera_sta = 0;
    while (1) {
        camera_capture(lcd_panel, 0, 0, LCD_WIDTH, LCD_HEIGHT);      
        vTaskDelay(pdMS_TO_TICKS(10));
        camera_sta++;
        if (camera_sta == 200) {
            camera_save_bmp_to_sdcard();
            camera_sta = 0;
        }
    }
}
