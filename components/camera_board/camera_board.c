#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "lcd_board.h"
#include "camera_board.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdio.h"

static const char *TAG = "camera_board";
static SemaphoreHandle_t s_camera_mutex = NULL;

#define CAMERA_XCLK_FREQ_HZ    (20 * 1000 * 1000)
#define CAMERA_JPEG_QUALITY    2
#define CAMERA_JPEG_RETRY_MAX  3

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .sccb_i2c_port = 0,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = CAMERA_XCLK_FREQ_HZ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,

    .jpeg_quality = CAMERA_JPEG_QUALITY,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

// 校验 JPEG 帧是否完整：检查 SOI(0xFFD8) 和 EOI(0xFFD9) 标记
static bool camera_frame_is_complete_jpeg(const camera_fb_t *fb)
{
    return fb &&
           fb->format == PIXFORMAT_JPEG &&
           fb->len >= 4 &&
           fb->buf[0] == 0xFF &&                // JPEG SOI marker byte 0
           fb->buf[1] == 0xD8 &&                // JPEG SOI marker byte 1
           fb->buf[fb->len - 2] == 0xFF &&      // JPEG EOI marker byte 0
           fb->buf[fb->len - 1] == 0xD9;        // JPEG EOI marker byte 1
}

esp_err_t camera_init(camera_format_t format) {
    if (s_camera_mutex == NULL) {
        s_camera_mutex = xSemaphoreCreateMutex();
        if (s_camera_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create camera mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    switch (format) {
        case RGB565:
            // LCD 预览模式：双缓冲 + 取最新帧，保证画面流畅
            camera_config.pixel_format = PIXFORMAT_RGB565;
            camera_config.fb_count = 2;
            camera_config.grab_mode = CAMERA_GRAB_LATEST;
            break;
        case JPEG:
            // JPEG 拍照模式：单缓冲 + 等到非空才返回，保证帧完整性
            // 切换后 LCD 会暂时黑屏，拍完需切回 RGB565 恢复预览
            camera_config.pixel_format = PIXFORMAT_JPEG;
            camera_config.jpeg_quality = CAMERA_JPEG_QUALITY;
            camera_config.fb_count = 1;
            camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
            break;
        default:
            ESP_LOGE(TAG, "Invalid camera format");
            return ESP_FAIL;
    }

    if (camera_config.pin_pwdn >= 0) {
        gpio_set_level(camera_config.pin_pwdn, 0);
    }

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t camera_capture(esp_lcd_panel_handle_t panel, int x0, int y0, int x1, int y1){
    // 非阻塞拿锁：若摄像头正被 JPEG 保存占用，跳过本帧
    if (xSemaphoreTake(s_camera_mutex, 0) != pdTRUE) {
        return ESP_FAIL;
    }

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        xSemaphoreGive(s_camera_mutex);
        return ESP_FAIL;
    }

    esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, fb->buf);
    esp_camera_fb_return(fb);
    xSemaphoreGive(s_camera_mutex);
    return ESP_OK;
}

// 保存 JPEG 照片到 SD 卡。需要将摄像头从 RGB565 临时切换到 JPEG 模式再切回，
// 切换期间 LCD 会短暂黑屏（约 200~400ms）。文件小（10~30KB），适合需要节省 SD 卡空间的场景。
esp_err_t camera_save_jpeg_to_sdcard(void) {
    // 阻塞等待摄像头空闲（app_main capture loop 会跳过）
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);

    // 1. 反初始化 RGB565 模式，重新以 JPEG 模式初始化
    esp_camera_deinit();
    esp_err_t err = camera_init(JPEG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize camera in JPEG mode");
        xSemaphoreGive(s_camera_mutex);
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(100));     // 等待摄像头稳定

    // 2. 抓取 JPEG 帧，带重试和完整性校验
    camera_fb_t * fb = NULL;
    for (int retry = 0; retry < CAMERA_JPEG_RETRY_MAX; retry++) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera Capture Failed");
            err = ESP_FAIL;
            goto restore_rgb565;
        }

        if (camera_frame_is_complete_jpeg(fb)) {
            break;      // JPEG 帧完整，跳出重试循环
        }

        ESP_LOGW(TAG, "Invalid JPEG frame, retry %d/%d, len=%u, format=%d",
                 retry + 1, CAMERA_JPEG_RETRY_MAX, (unsigned)fb->len, fb->format);
        esp_camera_fb_return(fb);
        fb = NULL;
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    if (!fb) {
        err = ESP_FAIL;
        goto restore_rgb565;
    }

    // 3. 写入 SD 卡（JPEG 数据已由摄像头硬件编码，直接存即可）
    static int img_count = 0;
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/img_%d.jpg", img_count++);

    err = sdio_write_binary_file(path, fb->buf, fb->len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write JPEG to SD card");
        esp_camera_fb_return(fb);
        goto restore_rgb565;
    }

    esp_camera_fb_return(fb);

restore_rgb565:
    // 4. 切回 RGB565 模式，恢复 LCD 预览
    esp_camera_deinit();
    esp_err_t restore_err = camera_init(RGB565);
    if (restore_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize camera in RGB565 mode");
        xSemaphoreGive(s_camera_mutex);
        return restore_err;
    }
    xSemaphoreGive(s_camera_mutex);
    return err;
}

/* ================================================================
 *  BMP24 保存 — 摄像头保持 RGB565 模式，无需切换，LCD 预览不中断
 *
 *  BMP 文件结构：
 *    BITMAPFILEHEADER   14 bytes   bfType='BM', bfSize, bfOffBits=54
 *    BITMAPINFOHEADER   40 bytes   biWidth, biHeight, biBitCount=24, biCompression=BI_RGB
 *    像素数据            width*height*3 bytes   底行在前，BGR 顺序
 *
 *  摄像头输出大端序 RGB565（与 ILI9341 屏幕时序一致），字节布局：
 *    byte0 = RRRRRGGG,  byte1 = GGGBBBBB
 *  因此不能直接用 uint16_t* 读取（ESP32 小端序会交换高低字节），
 *  必须逐字节解析。
 *
 *  每张 BMP 约 230KB（320×240×3 + 54 头）。
 * ================================================================ */

// 将大端序 RGB565 的两个字节转换为 BMP 所需的 BGR888（3 字节）
static void rgb565be_to_bgr888(const uint8_t *src, uint8_t bgr[3])
{
    uint8_t hi = src[0];  // RRRRRGGG
    uint8_t lo = src[1];  // GGGBBBBB

    // 从大端序字节中提取 5-6-5 分量
    uint8_t r5 = hi >> 3;                       // R[4:0] = hi[7:3]
    uint8_t g6 = ((hi & 0x07) << 3) | (lo >> 5); // G[5:0] = hi[2:0] << 3 | lo[7:5]
    uint8_t b5 = lo & 0x1F;                     // B[4:0] = lo[4:0]

    // 5/6 位 → 8 位扩展，写入 BGR 顺序
    bgr[0] = (b5 << 3) | (b5 >> 2);   // B
    bgr[1] = (g6 << 2) | (g6 >> 4);   // G
    bgr[2] = (r5 << 3) | (r5 >> 2);   // R
}

// 保存 BMP24 照片到 SD 卡。摄像头保持 RGB565 模式不变，
// 抓取当前帧后在内存中转换为 BMP24 格式再写入 SD 卡。
esp_err_t camera_save_bmp_to_sdcard(void)
{
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);

    // 1. 抓取当前 RGB565 帧
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        xSemaphoreGive(s_camera_mutex);
        return ESP_FAIL;
    }

    int width  = fb->width;
    int height = fb->height;
    int row_size = width * 3;                   // BGR888 每行字节数
    int padding = (4 - (row_size & 3)) & 3;     // BMP 要求每行 4 字节对齐
    int pixel_data_size = (row_size + padding) * height;
    int file_size = 54 + pixel_data_size;       // 54 = 14(文件头) + 40(信息头)

    // 2. 分配 BMP 文件缓冲区
    uint8_t *bmp = malloc(file_size);
    if (!bmp) {
        ESP_LOGE(TAG, "Malloc BMP buffer failed");
        esp_camera_fb_return(fb);
        xSemaphoreGive(s_camera_mutex);
        return ESP_FAIL;
    }
    memset(bmp, 0, file_size);

    // 3. 填充 BITMAPFILEHEADER（14 bytes）
    bmp[0]  = 'B';
    bmp[1]  = 'M';                            // bfType = "BM"
    bmp[2]  = (file_size >> 0)  & 0xFF;       // bfSize（4 bytes 小端）
    bmp[3]  = (file_size >> 8)  & 0xFF;
    bmp[4]  = (file_size >> 16) & 0xFF;
    bmp[5]  = (file_size >> 24) & 0xFF;
    // bmp[6..9] = 0                          // bfReserved1/2（已 memset）
    bmp[10] = 54;                             // bfOffBits = 像素数据起始偏移

    // 4. 填充 BITMAPINFOHEADER（40 bytes）
    bmp[14] = 40;                             // biSize
    bmp[18] = (width >> 0)  & 0xFF;           // biWidth（4 bytes 小端）
    bmp[19] = (width >> 8)  & 0xFF;
    bmp[22] = (height >> 0) & 0xFF;           // biHeight（正数 = 底行在前）
    bmp[23] = (height >> 8) & 0xFF;
    bmp[26] = 1;                              // biPlanes
    bmp[28] = 24;                             // biBitCount
    // bmp[30..53] = 0                        // biCompression=BI_RGB, 其余为 0（已 memset）

    // 5. 像素转换：RGB565 BE → BGR888，底行在前（BMP 默认行序）
    uint8_t *src = fb->buf;
    for (int y = 0; y < height; y++) {
        int src_row = height - 1 - y;         // BMP 第 y 行 ← 摄像头第 (height-1-y) 行
        uint8_t *dst = bmp + 54 + y * row_size;
        for (int x = 0; x < width; x++) {
            rgb565be_to_bgr888(&src[(src_row * width + x) * 2], &dst[x * 3]);
        }
    }

    esp_camera_fb_return(fb);                 // 归还帧缓冲，像素数据已拷贝

    // 6. 写入 SD 卡
    static int bmp_count = 0;
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/img_%d.bmp", bmp_count++);

    esp_err_t err = sdio_write_binary_file(path, bmp, file_size);
    free(bmp);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write BMP to SD card");
        xSemaphoreGive(s_camera_mutex);
        return err;
    }

    ESP_LOGI(TAG, "BMP saved: %s (%d bytes)", path, file_size);
    xSemaphoreGive(s_camera_mutex);
    return ESP_OK;
}
