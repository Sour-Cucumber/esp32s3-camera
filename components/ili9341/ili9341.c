#include <stdlib.h>
#include <sys/cdefs.h>
#include "sdkconfig.h"

#if CONFIG_LCD_ENABLE_DEBUG_LOG
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9341.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_compiler.h"

/* ILI9341 特定寄存器 */
#define ILI9341_CMD_FRMCTR1          0xB1    /* 帧率控制 (正常模式) */
#define ILI9341_CMD_FRMCTR2          0xB2    /* 帧率控制 (空闲模式) */
#define ILI9341_CMD_FRMCTR3          0xB3    /* 帧率控制 (部分模式) */
#define ILI9341_CMD_INVCTRL          0xB4    /* 显示反转控制 */
#define ILI9341_CMD_DFUNCTRL         0xB6    /* 显示功能控制 */
#define ILI9341_CMD_PWCTRL1          0xC0    /* 电源控制 1 */
#define ILI9341_CMD_PWCTRL2          0xC1    /* 电源控制 2 */
#define ILI9341_CMD_VMCTRL1          0xC5    /* VCOM 控制 1 */
#define ILI9341_CMD_VMCTRL2          0xC7    /* VCOM 控制 2 */
#define ILI9341_CMD_PWCTRLA          0xCB    /* 电源控制 A */
#define ILI9341_CMD_PWCTRLB          0xCF    /* 电源控制 B */
#define ILI9341_CMD_PGAMMAC          0xE0    /* 正 Gamma 校正 */
#define ILI9341_CMD_NGAMMAC          0xE1    /* 负 Gamma 校正 */
#define ILI9341_CMD_DTCTRLA          0xE8    /* 驱动时序控制 A */
#define ILI9341_CMD_DTCTRLB          0xEA    /* 驱动时序控制 B */
#define ILI9341_CMD_PWRSEQ           0xED    /* 上电时序控制 */
#define ILI9341_CMD_PRCTRL           0xF7    /* 泵比控制 */

static const char *TAG = "lcd_panel.ili9341";

static esp_err_t panel_ili9341_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                           int x_end, int y_end, const void *color_data);
static esp_err_t panel_ili9341_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_ili9341_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ili9341_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_ili9341_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_ili9341_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_ili9341_sleep(esp_lcd_panel_t *panel, bool sleep);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
} ili9341_panel_t;

esp_err_t esp_lcd_new_panel_ili9341(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
#if CONFIG_LCD_ENABLE_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    esp_err_t ret = ESP_OK;
    ili9341_panel_t *ili9341 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE("-Wanalyzer-malloc-leak")
    ili9341 = calloc(1, sizeof(ili9341_panel_t));
    ESP_GOTO_ON_FALSE(ili9341, ESP_ERR_NO_MEM, err, TAG, "no mem for ili9341 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        ili9341->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        ili9341->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported RGB element order");
        break;
    }

    uint8_t fb_bits_per_pixel = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        ili9341->colmod_val = 0x55;
        fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        ili9341->colmod_val = 0x66;
        fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    ili9341->io = io;
    ili9341->fb_bits_per_pixel = fb_bits_per_pixel;
    ili9341->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ili9341->reset_level = panel_dev_config->flags.reset_active_high;
    ili9341->base.del = panel_ili9341_del;
    ili9341->base.reset = panel_ili9341_reset;
    ili9341->base.init = panel_ili9341_init;
    ili9341->base.draw_bitmap = panel_ili9341_draw_bitmap;
    ili9341->base.invert_color = panel_ili9341_invert_color;
    ili9341->base.set_gap = panel_ili9341_set_gap;
    ili9341->base.mirror = panel_ili9341_mirror;
    ili9341->base.swap_xy = panel_ili9341_swap_xy;
    ili9341->base.disp_on_off = panel_ili9341_disp_on_off;
    ili9341->base.disp_sleep = panel_ili9341_sleep;
    *ret_panel = &(ili9341->base);
    ESP_LOGD(TAG, "new ili9341 panel @%p", ili9341);

    return ESP_OK;

err:
    if (ili9341) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ili9341);
    }
    return ret;
    ESP_COMPILER_DIAGNOSTIC_POP("-Wanalyzer-malloc-leak")
}

static esp_err_t panel_ili9341_del(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    if (ili9341->reset_gpio_num >= 0) {
        gpio_reset_pin(ili9341->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del ili9341 panel @%p", ili9341);
    free(ili9341);
    return ESP_OK;
}

static esp_err_t panel_ili9341_reset(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;

    if (ili9341->reset_gpio_num >= 0) {
        gpio_set_level(ili9341->reset_gpio_num, ili9341->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ili9341->reset_gpio_num, !ili9341->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG,
                            "io tx param failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_ili9341_init(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;

    /* 退出休眠 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "slpout");

    /* 电源控制 A */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PWCTRLA, (uint8_t[]) {
        0x39, 0x2C, 0x00, 0x34, 0x02
    }, 5), TAG, "pwctrla");

    /* 电源控制 B */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PWCTRLB, (uint8_t[]) {
        0x00, 0xC1, 0x30
    }, 3), TAG, "pwctrlb");

    /* 驱动时序控制 A */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_DTCTRLA, (uint8_t[]) {
        0x85, 0x00, 0x78
    }, 3), TAG, "dtctrla");

    /* 驱动时序控制 B */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_DTCTRLB, (uint8_t[]) {
        0x00, 0x00
    }, 2), TAG, "dtctrlb");

    /* 上电时序控制 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PWRSEQ, (uint8_t[]) {
        0x64, 0x03, 0x12, 0x81
    }, 4), TAG, "pwrseq");

    /* 泵比控制 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PRCTRL, (uint8_t[]) {
        0x20
    }, 1), TAG, "prctrl");

    /* 电源控制 1 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PWCTRL1, (uint8_t[]) {
        0x23
    }, 1), TAG, "pwctrl1");

    /* 电源控制 2 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PWCTRL2, (uint8_t[]) {
        0x10
    }, 1), TAG, "pwctrl2");

    /* VCOM 控制 1 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_VMCTRL1, (uint8_t[]) {
        0x3E, 0x28
    }, 2), TAG, "vmctrl1");

    /* VCOM 控制 2 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_VMCTRL2, (uint8_t[]) {
        0x86
    }, 1), TAG, "vmctrl2");

    /* 内存访问控制 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9341->madctl_val
    }, 1), TAG, "madctl");

    /* 像素格式 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        ili9341->colmod_val
    }, 1), TAG, "colmod");

    /* 帧率控制 (正常模式) */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_FRMCTR1, (uint8_t[]) {
        0x00, 0x1B
    }, 2), TAG, "frmctr1");

    /* 显示功能控制 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_DFUNCTRL, (uint8_t[]) {
        0x08, 0x82, 0x27
    }, 3), TAG, "dfunctrl");

    /* Gamma 曲线选择: G2.2 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_GAMSET, (uint8_t[]) {
        0x01
    }, 1), TAG, "gammaset");

    /* 正 Gamma 校正 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_PGAMMAC, (uint8_t[]) {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    }, 15), TAG, "pgammac");

    /* 负 Gamma 校正 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ILI9341_CMD_NGAMMAC, (uint8_t[]) {
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    }, 15), TAG, "ngammac");

    vTaskDelay(pdMS_TO_TICKS(10));

    /* 睡眠退出后显示可能处于关闭态，确保进入正常显示模式 */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_NORON, NULL, 0), TAG, "normon");

    return ESP_OK;
}

static esp_err_t panel_ili9341_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                           int x_end, int y_end, const void *color_data)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;

    x_start += ili9341->x_gap;
    x_end += ili9341->x_gap;
    y_start += ili9341->y_gap;
    y_end += ili9341->y_gap;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "caset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "raset");

    size_t len = (x_end - x_start) * (y_end - y_start) * ili9341->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "tx color");

    return ESP_OK;
}

static esp_err_t panel_ili9341_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "invert");
    return ESP_OK;
}

static esp_err_t panel_ili9341_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;
    if (mirror_x) {
        ili9341->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        ili9341->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        ili9341->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        ili9341->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9341->madctl_val
    }, 1), TAG, "mirror");
    return ESP_OK;
}

static esp_err_t panel_ili9341_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;
    if (swap_axes) {
        ili9341->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        ili9341->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9341->madctl_val
    }, 1), TAG, "swap_xy");
    return ESP_OK;
}

static esp_err_t panel_ili9341_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    ili9341->x_gap = x_gap;
    ili9341->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ili9341_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;
    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "disp_on_off");
    return ESP_OK;
}

static esp_err_t panel_ili9341_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;
    int command = sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "sleep");
    if (!sleep) {
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}
