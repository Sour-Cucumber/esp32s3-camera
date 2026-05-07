#include "lcd_board.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "tca9554.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd_board";

#define LCD_SPI_HOST    SPI2_HOST
#define LCD_MOSI_GPIO   0
#define LCD_SCLK_GPIO   1
#define LCD_DC_GPIO     2
#define LCD_BL_TCAGPIO  TCA9554_GPIO_NUM_1
#define LCD_RST_TCAGPIO TCA9554_GPIO_NUM_2
#define LCD_CS_TCAGPIO  TCA9554_GPIO_NUM_3

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

/*
 * 初始化流程 (按顺序):
 *   1. TCA9554 引脚配置   — 将 CS/RST/BL 设为输出并拉至初始电平
 *   2. ILI9341 硬件复位   — RST 低 10ms → 高 120ms，满足数据手册时序
 *   3. SPI 总线初始化     — MOSI=IO0, SCLK=IO1, 单向(无 MISO), DMA 缓冲 = 一帧+8B
 *   4. Panel IO 创建      — D/C=IO2, SPI Mode 0, 40MHz, CS/RST 由 TCA9554 手动管理
 *   5. 手工拉低 CS        — 驱动不管理 CS (cs_gpio_num=-1)，这里手工片选
 *   6. Panel 设备创建     — 16bpp RGB565, 硬件复位在本步骤前已完成
 *   7. 软件复位+初始化    — 发送软复位、出厂序列、开显示、开背光
 *
 * 设计要点:
 *   CS/RST/BL 走 TCA9554 (I2C GPIO 扩展) 而非 ESP32 直连 GPIO —
 *   三根线共享 I2C 总线，节省 ESP32 物理引脚，低速信号延迟可忽略。
 *   MOSI/SCLK/DC 必须走直连 GPIO，因为它们是高速信号 (40MHz SPI)。
 */
esp_err_t lcd_board_init(esp_lcd_panel_handle_t *out_panel)
{
    /* ---- 1. 配置 TCA9554 引脚方向与初始电平 ---- */
    tca9554_set_io_config(LCD_BL_TCAGPIO, TCA9554_IO_OUTPUT);
    tca9554_set_io_config(LCD_RST_TCAGPIO, TCA9554_IO_OUTPUT);
    tca9554_set_io_config(LCD_CS_TCAGPIO, TCA9554_IO_OUTPUT);

    tca9554_set_output_level(LCD_CS_TCAGPIO, TCA9554_IO_HIGH);  // CS 高 → 未选中
    tca9554_set_output_level(LCD_RST_TCAGPIO, TCA9554_IO_LOW);  // RST 低 → 复位
    tca9554_set_output_level(LCD_BL_TCAGPIO, TCA9554_IO_LOW);   // BL 低 → 背光灭

    /* ---- 2. ILI9341 硬件复位 (≥10ms 低 → ≥120ms 高) ---- */
    vTaskDelay(pdMS_TO_TICKS(10));
    tca9554_set_output_level(LCD_RST_TCAGPIO, TCA9554_IO_HIGH);
    vTaskDelay(pdMS_TO_TICKS(120));  // 等待内部电源稳定

    /* ---- 3. SPI 总线初始化 ---- */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_MOSI_GPIO,       // IO0
        .miso_io_num = -1,                  // 写多读少 → 禁用 MISO
        .sclk_io_num = LCD_SCLK_GPIO,       // IO1
        .quadwp_io_num = -1,                // 未使用 QSPI
        .quadhd_io_num = -1,                // 未使用 QSPI
        .max_transfer_sz = 320 * 240 * 2 + 8, // 一帧像素 + 指令/参数开销
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* ---- 4. 创建 Panel IO (SPI 传输协议层) ---- */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1,                   // CS 在 TCA9554 上，驱动不管理
        .dc_gpio_num = LCD_DC_GPIO,          // D/C=IO2, 高速信号需直连 GPIO
        .spi_mode = 0,                       // CPOL=0 CPHA=0, 符合 ILI9341 要求
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,       // SPI 时钟 40MHz
        .trans_queue_depth = 10,             // 事务队列深度
        .lcd_cmd_bits = 8,                   // ILI9341 命令字长 8 位
        .lcd_param_bits = 8,                 // ILI9341 参数字长 8 位
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io_handle));

    /* ---- 5. 手工片选 ILI9341 ---- */
    tca9554_set_output_level(LCD_CS_TCAGPIO, TCA9554_IO_LOW);

    /* ---- 6. 创建 ILI9341 Panel 设备 ---- */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,                     // 硬件复位已通过 TCA9554 完成
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,                     // RGB565, 每像素 2 字节
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_cfg, out_panel));

    /* ---- 7. 软件复位 → 出厂初始化序列 → 开显示 → 开背光 ---- */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*out_panel));      // 软复位命令 0x01
    ESP_ERROR_CHECK(esp_lcd_panel_init(*out_panel));       // 发送出厂序列 (sleep out, 像素格式, 伽马曲线等)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*out_panel, true)); // Display ON (0x29)

    lcd_board_set_backlight(true);  // TCA9554 P1 高 → 背光亮

    ESP_LOGI(TAG, "LCD board initialized");
    return ESP_OK;
}

esp_err_t lcd_board_set_backlight(bool on)
{
    return tca9554_set_output_level(LCD_BL_TCAGPIO, on ? TCA9554_IO_HIGH : TCA9554_IO_LOW);
}
