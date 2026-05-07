#include "lcd_board.h"
#include "tca9554.h"
#include "ili9341.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "lcd_board";

/* ── 引脚定义 ── */
#define SPI_MOSI_IO         0       /* SDA */
#define SPI_SCLK_IO         1       /* CLK */
#define SPI_DC_IO           2       /* DC (命令/数据选择) */

#define TCA9554_PIN_BL      TCA9554_GPIO_NUM_1   /* 背光 CTRL */
#define TCA9554_PIN_RST     TCA9554_GPIO_NUM_2   /* 复位 */
#define TCA9554_PIN_CS      TCA9554_GPIO_NUM_3   /* SPI 片选 */

/* ILI9341 面板尺寸 */
#define LCD_H_RES            320
#define LCD_V_RES            240

/* 用于记录面板句柄，供背光控制等后续操作使用 */
static esp_lcd_panel_handle_t s_panel = NULL;

esp_err_t lcd_board_init(esp_lcd_panel_handle_t *out_panel)
{
    esp_err_t ret;

    /* ── 1. 通过 TCA9554 配置控制引脚 ── */
    ESP_LOGI(TAG, "Configuring TCA9554 control pins");
    ESP_RETURN_ON_ERROR(tca9554_set_io_config(TCA9554_PIN_BL, TCA9554_IO_OUTPUT), TAG, "cfg BL");
    ESP_RETURN_ON_ERROR(tca9554_set_io_config(TCA9554_PIN_RST, TCA9554_IO_OUTPUT), TAG, "cfg RST");
    ESP_RETURN_ON_ERROR(tca9554_set_io_config(TCA9554_PIN_CS, TCA9554_IO_OUTPUT), TAG, "cfg CS");

    /* ── 2. CS 拉低 (单 SPI 设备的常规做法，常选通) ── */
    ESP_RETURN_ON_ERROR(tca9554_set_output_level(TCA9554_PIN_CS, TCA9554_IO_LOW), TAG, "cs low");

    /* ── 3. 硬件复位 LCD (RST: 高 → 低 → 高) ── */
    ESP_LOGI(TAG, "Resetting LCD panel");
    ESP_RETURN_ON_ERROR(tca9554_set_output_level(TCA9554_PIN_RST, TCA9554_IO_HIGH), TAG, "rst high");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tca9554_set_output_level(TCA9554_PIN_RST, TCA9554_IO_LOW), TAG, "rst low");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tca9554_set_output_level(TCA9554_PIN_RST, TCA9554_IO_HIGH), TAG, "rst high");
    vTaskDelay(pdMS_TO_TICKS(120));

    /* ── 4. 初始化 SPI 总线 ── */
    ESP_LOGI(TAG, "Initializing SPI bus (MOSI=%d, SCLK=%d, DC=%d)", SPI_MOSI_IO, SPI_SCLK_IO, SPI_DC_IO);
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SPI_MOSI_IO,
        .miso_io_num = -1,
        .sclk_io_num = SPI_SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "spi bus init");

    /* ── 5. 创建 SPI 面板 IO (CS 由 TCA9554 控制，这里传 -1) ── */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = SPI_DC_IO,
        .cs_gpio_num = -1,          /* 不控制 CS，已由 TCA9554 常低 */
        .pclk_hz = 40 * 1000 * 1000,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle), TAG, "panel io");

    /* ── 6. 创建 ILI9341 面板 ── */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,       /* 不控制 RST，已由 TCA9554 复位 */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,       /* RGB565 */
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &s_panel), err, TAG, "new panel");

    /* ── 7. 初始化面板 (发送 ILI9341 初始化序列) ── */
    ESP_LOGI(TAG, "Initializing ILI9341 panel");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(s_panel), err, TAG, "panel init");

    /* ── 8. 点亮屏幕 ── */
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), err, TAG, "disp on");

    /* ── 9. 打开背光 (非致命，失败不滚回面板) ── */
    ret = tca9554_set_output_level(TCA9554_PIN_BL, TCA9554_IO_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to turn on backlight, panel may be working without backlight");
    }

    *out_panel = s_panel;
    ESP_LOGI(TAG, "LCD board initialized successfully");
    return ESP_OK;

err:
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    if (s_panel) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    spi_bus_free(SPI2_HOST);
    return ret;
}

esp_err_t lcd_board_set_backlight(uint8_t percent)
{
    if (percent > 0) {
        return tca9554_set_output_level(TCA9554_PIN_BL, TCA9554_IO_HIGH);
    } else {
        return tca9554_set_output_level(TCA9554_PIN_BL, TCA9554_IO_LOW);
    }
}
