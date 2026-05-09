● 你的项目正好是一个 esp32-camera 的实际例子。下面以这个项目为基础，介绍 esp32-camera 常用的 API。                                                     
                                                                                                                                                       
  ---                                                                                                                                                  
  1. 头文件与初始化                                                                                                                                    
                                                                                                                                                       
  #include "esp_camera.h"                                                                                                                              
                                                                                                                                                       
  esp_camera_init()

  初始化摄像头，这是使用所有其他 API 的前提。

  esp_err_t esp_camera_init(const camera_config_t *config);

  你的项目第 76 行就这么用的：

  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Camera Init Failed");
      return err;
  }

  ---
  2. 核心配置结构体 camera_config_t

  这是最关键的配置，定义在 esp_camera.h 中（你项目第 42-71 行是一个完整示例）：

  ┌───────────────────────────────┬─────────────────────────────────────────────────────────────────┐
  │             字段              │                              说明                                │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_pwdn                     │ 摄像头掉电使能引脚                                                │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_reset                    │ 复位引脚                                                         │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_xclk                     │ XCLK 时钟输出引脚                                                │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_sscb_sda / .pin_sscb_scl │ SCCB（类似 I2C）数据/时钟引脚，用于配置摄像头寄存器                 │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_d7 ~ .pin_d0             │ 8 位并行数据引脚                                                 │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_vsync                    │ 帧同步（垂直同步）信号                                            │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_href                     │ 行同步（水平参考）信号                                            │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pin_pclk                     │ 像素时钟输入                                                     │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .xclk_freq_hz                 │ XCLK 频率，通常 10MHz 或 20MHz。你项目设的是 40000000（40MHz）     │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .ledc_timer / .ledc_channel   │ 用于生成 XCLK 的 LEDC 定时器和通道                                │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .pixel_format                 │ 像素格式（见下方）                                                │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .frame_size                   │ 分辨率（见下方）                                                  │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .jpeg_quality                 │ JPEG 质量（0-63，越小质量越高），仅在 JPEG 格式时有效               │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .fb_count                     │ Frame Buffer 数量，设为 2 时 I2S 进入连续模式。仅 JPEG 模式使用    │
  ├───────────────────────────────┼─────────────────────────────────────────────────────────────────┤
  │ .grab_mode                    │ 抓取模式：CAMERA_GRAB_WHEN_EMPTY 或 CAMERA_GRAB_LATEST           │
  └───────────────────────────────┴─────────────────────────────────────────────────────────────────┘

  ---
  3. 像素格式 (pixformat_t)

  typedef enum {
      PIXFORMAT_RGB565,    // 每个像素 2 字节，RGB565 格式
      PIXFORMAT_YUV422,    // YUV 4:2:2 格式
      PIXFORMAT_YUV420,    // YUV 4:2:0 格式
      PIXFORMAT_GRAYSCALE, // 灰度图
      PIXFORMAT_JPEG,      // JPEG 压缩格式（最常用，节省内存）
      PIXFORMAT_RGB888,    // 每个像素 3 字节
      PIXFORMAT_RAW,       // 原始 RAW 数据
      PIXFORMAT_PIXFORMAT_MAX,
  } pixformat_t;

  你项目里用了 PIXFORMAT_YUV422 和 PIXFORMAT_RGB565 两种模式（第 34-38 行），实际最常用的是 PIXFORMAT_JPEG（适合拍照、WiFi 传输等场景）。

  ---
  4. 帧大小 (framesize_t)

  typedef enum {
      FRAMESIZE_96X96,    // 96x96
      FRAMESIZE_QQVGA,    // 160x120
      FRAMESIZE_QCIF,     // 176x144
      FRAMESIZE_HQVGA,    // 240x176
      FRAMESIZE_240X240,  // 240x240
      FRAMESIZE_QVGA,     // 320x240  ← 你项目用的是这个
      FRAMESIZE_CIF,      // 400x296
      FRAMESIZE_HVGA,     // 480x320
      FRAMESIZE_VGA,      // 640x480
      FRAMESIZE_SVGA,     // 800x600
      FRAMESIZE_XGA,      // 1024x768
      FRAMESIZE_HD,       // 1280x720
      FRAMESIZE_SXGA,     // 1280x1024
      FRAMESIZE_UXGA,     // 1600x1200
      FRAMESIZE_QXGA,     // 2048x1536  ← OV3660 最高支持
  } framesize_t;

  注意注释里的建议：非 JPEG 模式下不要超过 QVGA，否则帧缓冲区太大，内存吃不消。

  ---
  5. 获取与归还帧

  esp_camera_fb_get() — 获取一帧

  camera_fb_t *esp_camera_fb_get(void);

  返回 camera_fb_t 结构体指针：

  typedef struct {
      uint8_t *buf;     // 图像数据缓冲区
      size_t len;       // 数据长度（字节）
      size_t width;     // 图像宽度
      size_t height;    // 图像高度
      pixformat_t format; // 像素格式
      // ...
  } camera_fb_t;

  你项目第 123 行：
  camera_fb_t *pic = esp_camera_fb_get();
  ESP_LOGI(TAG, "Picture taken! size:%zu bytes, w:%d, h:%d", pic->len, pic->width, pic->height);

  esp_camera_fb_return() — 归还帧缓冲区

  void esp_camera_fb_return(camera_fb_t *fb);

  非常重要——每次 _fb_get() 之后必须调用 _fb_return() 归还，否则帧缓冲区会耗尽。你项目第 136 行在循环末尾正确处理了这个问题。

  ---
  6. 辅助函数

  esp_camera_deinit() — 反初始化

  esp_err_t esp_camera_deinit(void);

  释放摄像头资源，通常在不需要使用时调用。

  esp_camera_sensor_get() — 获取 sensor 对象

  sensor_t *esp_camera_sensor_get(void);

  返回 sensor 指针，可以用它来直接操作 sensor 的底层寄存器，比如：
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);         // 垂直翻转
  s->set_hmirror(s, 1);       // 水平镜像
  s->set_brightness(s, 2);    // 亮度
  s->set_contrast(s, 0);      // 对比度
  s->set_saturation(s, 0);    // 饱和度
  s->set_quality(s, 30);      // JPEG 质量
  s->set_framesize(s, FRAMESIZE_QVGA); // 分辨率

  ---
  7. 图像格式转换

| 函数          | 转换方向           | 说明 
| fmt2rgb888    | 任意格式 -> RGB888 | 通用转 RGB888，用于 LCD 显示 
| fmt2jpg       | 任意格式 -> JPEG   | 压缩为 JPEG，用于存储/传输 
| fmt2bmp       | 任意格式 -> BMP    | 转位图，PC 端直接查看 
| rgb5652jpg    | RGB565 -> JPEG    | 专用 RGB565 转 JPEG，比 fmt2jpg 快 
| jpeg2rgb888   | JPEG -> RGB888    | JPEG 解压为 RGB888，scale 可缩小(1/2/4/8) 
| rgb8882jpeg   | RGB888 -> JPEG    | 专用 RGB888 转 JPEG 
| bmp2rgb888    | BMP -> RGB888     | BMP 解码 
| rgb8882rgb565 | RGB888 -> RGB565  | 简单像素格式转换 

  ---
  总结：典型使用流程

  1. 填充 camera_config_t 结构体
  2. esp_camera_init(&config)           → 初始化摄像头
  3. 循环中：
     a. esp_camera_fb_get()             → 获取一帧
     b. 使用 pic->buf 做处理（显示/存储/发送）
     c. esp_camera_fb_return(pic)       → 归还帧缓冲区
  4. esp_camera_deinit()                → 反初始化（如需）

  你的 lcd_camera_main.c 就是一个完整的演示——摄像头初始化 → 循环取帧 → 显示到 LCD → 归还帧缓冲区。