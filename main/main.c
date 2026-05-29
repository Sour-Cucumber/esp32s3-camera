#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "tca9554.h"
#include "lcd_board.h"
#include "camera_board.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_camera.h"
#include "sdio.h"
#include "board_buttons.h"

#define LED_RED_NUM                 TCA9554_GPIO_NUM_6     // 红色 LED 对应tca9554引脚6
#define LED_BLUE_NUM                TCA9554_GPIO_NUM_7     // 蓝色 LED 对应tca9554

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

static esp_lcd_panel_handle_t lcd_panel;
static QueueHandle_t button_event_queue;

static void button_cb(void *btn_handle, void *usr_data)
{
    int btn_id = (int)usr_data;
    xQueueSend(button_event_queue, &btn_id, portMAX_DELAY);
}

static void button_task(void *arg)
{
    int btn_index = 0;
    while (1) {
        if (xQueueReceive(button_event_queue, &btn_index, portMAX_DELAY) == pdTRUE) {
            switch (btn_index) {
                case BOARD_BTN_REC:
                    // 处理 REC 按键事件
                    ESP_LOGI("button_task", "Received button event for button %s", btn_names[btn_index]);
                    break;
                case BOARD_BTN_MUTE:
                    // 处理 MUTE 按键事件
                    ESP_LOGI("button_task", "Received button event for button %s", btn_names[btn_index]);
                    break;
                case BOARD_BTN_PLAY:
                    // 处理 PLAY 按键事件
                    ESP_ERROR_CHECK(camera_save_bmp_to_sdcard());
                    ESP_LOGI("button_task", "Saved BMP photo to SD card");
                    break;
                case BOARD_BTN_SET:
                    // 处理 SET 按键事件
                    ESP_ERROR_CHECK(camera_save_jpeg_to_sdcard());
                    ESP_LOGI("button_task", "Saved JPEG photo to SD card");
                    break;
                case BOARD_BTN_VOL_DOWN:
                    // 处理 VOL- 按键事件
                    ESP_LOGI("button_task", "Received button event for button %s", btn_names[btn_index]);
                    break;
                case BOARD_BTN_VOL_UP:
                    // 处理 VOL+ 按键事件
                    ESP_LOGI("button_task", "Received button event for button %s", btn_names[btn_index]);
                    break;
                default:
                    ESP_LOGW("button_task", "Unknown button index: %d", btn_index);
            }
        }
    }
}

void app_main(void) {

    ESP_LOGI("main", "Initializing TCA9554");                                                                                                           
    ESP_ERROR_CHECK(tca9554_init());

    ESP_LOGI("main", "Initializing LCD");
    ESP_ERROR_CHECK(lcd_board_init(&lcd_panel));

    ESP_LOGI("main", "Initializing Camera");                                                                                                     
    ESP_ERROR_CHECK(camera_init(RGB565));   

    ESP_LOGI("main", "Initializing SD Card");
    sdcard_init();

    ESP_LOGI("main", "Initializing adc_button");
    ESP_ERROR_CHECK(board_buttons_init());


    for (int i = 0; i < BOARD_BTN_MAX; i++) {
        button_handle_t btn_handle = board_button_get_handle(i);
        ESP_ERROR_CHECK(iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, button_cb, (void *)i));
        ESP_LOGI("button_cb", "Registered callback for button %s", btn_names[i]);
    }

    button_event_queue = xQueueCreate(10, sizeof(int));
    assert (button_event_queue != NULL);

    ESP_LOGI("main", "Creating button task");
    BaseType_t ret = xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
    if (ret != pdPASS) {
        ESP_LOGE("main", "Failed to create button task");
        return;
    }

    while (1) {
        camera_capture(lcd_panel, 0, 0, LCD_WIDTH, LCD_HEIGHT);      
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
