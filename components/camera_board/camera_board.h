#ifndef __CAMERA_BOARD_H
#define __CAMERA_BOARD_H

#define CAM_PIN_PWDN                -1
#define CAM_PIN_RESET               -1
#define CAM_PIN_XCLK                GPIO_NUM_40
#define CAM_PIN_SIOD                -1          // 使用 TCA9554 已初始化的 I2C，不复用引脚
#define CAM_PIN_SIOC                -1

#define CAM_PIN_D7                  GPIO_NUM_39
#define CAM_PIN_D6                  GPIO_NUM_41
#define CAM_PIN_D5                  GPIO_NUM_42
#define CAM_PIN_D4                  GPIO_NUM_12
#define CAM_PIN_D3                  GPIO_NUM_3
#define CAM_PIN_D2                  GPIO_NUM_14
#define CAM_PIN_D1                  GPIO_NUM_47
#define CAM_PIN_D0                  GPIO_NUM_13
#define CAM_PIN_VSYNC               GPIO_NUM_21
#define CAM_PIN_HREF                GPIO_NUM_38
#define CAM_PIN_PCLK                GPIO_NUM_11

/*
 * brif: Initialize the camera
 */
esp_err_t camera_init();

/*
 * brif: Capture a frame from the camera and draw it on the LCD
 * param panel: LCD panel handle
 * param x0, y0, x1, y1: the area to draw the image on the LCD
 */
esp_err_t camera_capture(esp_lcd_panel_handle_t panel, int x0, int y0, int x1, int y1);

#endif 