# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

esp32s3-camera 是一个基于 ESP32-S3-Korvo2 的 ESP-IDF 项目，通过 I2C 总线驱动 TCA9554 GPIO 扩展芯片控制 ILI9341 LCD，DVP 接口驱动 OV2640 摄像头实时预览，ADC 按键拍照并保存到 SD 卡。

- **芯片**: ESP32-S3 (Korvo2 开发板)
- **框架**: ESP-IDF v5.5.3
- **I2C 扩展芯片**: TCA9554 (地址 0x20)
- **LCD 驱动**: ILI9341 (320×240, SPI, 16bpp RGB565)
- **LCD 驱动方案**: managed component `espressif/esp_lcd_ili9341`
- **摄像头**: OV2640 (DVP 并行接口, XCLK=20MHz, PIXFORMAT_RGB565, QVGA)
- **摄像头驱动方案**: managed component `espressif/esp32-camera` (^2.1.6)
- **按键驱动方案**: managed component `espressif__button` (v4.1.6)
- **SD 卡**: SDIO 模式
- **构建系统**: CMake

## 构建与烧录

**重要**: ESP-IDF 不支持 MSYS/MinGW (git bash) 环境。必须在 Windows CMD 或 PowerShell 中运行 `idf.py`。

```bash
idf.py build                         # 构建
idf.py -p COM3 flash                 # 烧录
idf.py -p COM3 monitor               # 串口监控
idf.py -p COM3 build flash monitor   # 一条龙
idf.py fullclean                     # 清理
```

## 架构

```
Jacktter/
├── main/                    # 主应用代码
│   ├── main.c               # 应用入口 + FreeRTOS 任务 + 采集循环
│   ├── CMakeLists.txt       # REQUIRES: tca9554 lcd_board camera_board esp32-camera sdcard board_buttons
│   └── idf_component.yml    # 声明 espressif/esp_lcd_ili9341, espressif/esp32-camera
├── components/              # 可复用的 ESP-IDF 组件
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h        # 驱动 API 头文件
│   │   ├── tca9554.c        # 驱动实现
│   │   └── CMakeLists.txt   # 依赖 driver (I2C)
│   ├── lcd_board/           # LCD 硬件初始化薄封装层
│   │   ├── lcd_board.h      # 公开 API（含 esp_lcd_panel_ops.h）
│   │   ├── lcd_board.c      # SPI + TCA9554 + ILI9341 初始化
│   │   ├── CMakeLists.txt   # REQUIRES: tca9554 driver esp_lcd esp_lcd_ili9341
│   │   └── idf_component.yml
│   ├── camera_board/        # 摄像头硬件抽象层 + BMP/JPEG 拍照 + 互斥锁
│   │   ├── camera_board.h   # 公开 API + 引脚定义
│   │   ├── camera_board.c   # esp_camera 初始化 + 帧采集 + 图片保存 + 互斥锁
│   │   └── CMakeLists.txt   # REQUIRES: driver esp32-camera lcd_board sdcard
│   ├── sdcard/              # SD 卡 SDIO 驱动
│   │   ├── sdio.h           # 公开 API
│   │   ├── sdio.c           # SDIO 初始化 + 文件写入
│   │   └── CMakeLists.txt   # REQUIRES: driver esp_driver_sdmmc fatfs vfs
│   └── board_buttons/       # 板载 6 键 ADC 按键封装
│       ├── board_buttons.h  # 公开 API + 按键枚举 + btn_names
│       ├── board_buttons.c  # ADC 按键初始化 (ADC1 CH4)
│       └── CMakeLists.txt   # REQUIRES: espressif__button driver esp_adc
├── managed_components/      # idf.py build 自动下载
│   ├── espressif__esp_lcd_ili9341/
│   ├── espressif__esp32-camera/
│   └── espressif__button/   # v4.1.6 (iot_button)
├── docs/
│   ├── esp32-camera_API参考.md
│   ├── espressif_button_guide.md
│   └── CLAUDE.md
├── .devcontainer/
├── CMakeLists.txt
└── sdkconfig
```

### 组件层次

```
main (app_main)
  ├── tca9554             — I2C GPIO 扩展
  ├── lcd_board           — SPI LCD 初始化
  ├── camera_board        — 摄像头 + 拍照
  ├── sdcard              — SD 卡存储
  └── board_buttons       — ADC 按键 (espressif__button)
        └── espressif__button > button_adc

components/tca9554
  ├── ESP-IDF driver (I2C)
  └── 公开 API: tca9554_init, tca9554_set_io_config, tca9554_get_io_config,
                tca9554_set_output_level, tca9554_get_output_level, tca9554_get_input_level

components/lcd_board
  ├── tca9554 (CS/RST/BL), esp_lcd (panel_ops, panel_io), espressif/esp_lcd_ili9341
  └── 公开 API: lcd_board_init, lcd_board_set_backlight

components/camera_board
  ├── espressif/esp32-camera, lcd_board (esp_lcd_panel_handle_t), sdcard
  ├── 公开 API: camera_init, camera_capture, camera_save_bmp_to_sdcard, camera_save_jpeg_to_sdcard
  └── 内部: s_camera_mutex (互斥锁), rgb565be_to_bgr888, camera_frame_is_complete_jpeg

components/sdcard
  ├── driver (esp_driver_sdmmc), fatfs, vfs
  └── 公开 API: sdcard_init, sdio_write_binary_file

components/board_buttons
  ├── espressif__button > button_adc
  ├── 公开 API: board_buttons_init, board_button_get_handle
  └── 公开数据: btn_names[BOARD_BTN_MAX] (按键名称数组)
```

### 多任务架构

```
app_main (优先级 1 — 主任务)
  └── while(1): camera_capture() → vTaskDelay(10ms)

button_task (优先级 10 — xTaskCreate 创建)
  └── while(1): xQueueReceive → switch(btn_index) → 拍照操作

按键回调 (esp_timer 上下文)
  └── xQueueSend → 唤醒 button_task
```

### 摄像头互斥锁机制

`camera_board` 内部使用 `SemaphoreHandle_t s_camera_mutex` 保护摄像头共享资源：

- **`camera_capture()`**: 非阻塞 `xSemaphoreTake(mutex, 0)` — 拿不到锁跳过本帧，不影响预览帧率
- **`camera_save_bmp_to_sdcard()`**: 阻塞 `xSemaphoreTake(mutex, portMAX_DELAY)` — 等待空闲后独占
- **`camera_save_jpeg_to_sdcard()`**: 阻塞拿锁 — 覆盖整个 deinit→JPEG→capture→save→RGB565 流程

所有 return 路径均释放锁，防止死锁。首次 `camera_init()` 调用时创建互斥锁。

### 应用主流程

```
app_main:
  1. tca9554_init()                  — 初始化 I2C 总线
  2. lcd_board_init(&lcd_panel)      — 初始化 LCD
  3. camera_init(RGB565)             — 初始化摄像头 (预览模式, fb_count=2, GRAB_LATEST)
  4. sdcard_init()                   — 初始化 SD 卡 (SDIO)
  5. board_buttons_init()            — 初始化 6 个 ADC 按键 (ADC1 CH4, GPIO5)
  6. 注册所有按键的 BUTTON_SINGLE_CLICK 回调 → button_cb
  7. xQueueCreate + xTaskCreate(button_task, prio=10)
  8. while(1):
       camera_capture() → vTaskDelay(10ms)

button_task:
  阻塞在 xQueueReceive → 收到 btn_id → switch:
    BOARD_BTN_PLAY → camera_save_bmp_to_sdcard()
    BOARD_BTN_SET  → camera_save_jpeg_to_sdcard()
    其他 → 仅日志
```

### ADC 按键 (ESP32-S3-Korvo2)

6 个按键共享 ADC1 CH4 (GPIO5)，通过分压电阻产生不同电压值区分：

| 按键 | 枚举 | 分压值 | 电压窗口 |
|------|------|--------|----------|
| REC | `BOARD_BTN_REC` | 380mV | 280-480mV |
| MUTE | `BOARD_BTN_MUTE` | 820mV | 720-920mV |
| PLAY | `BOARD_BTN_PLAY` | 1180mV | 1080-1280mV |
| SET | `BOARD_BTN_SET` | 1570mV | 1470-1670mV |
| VOL- | `BOARD_BTN_VOL_DOWN` | 1980mV | 1880-2080mV |
| VOL+ | `BOARD_BTN_VOL_UP` | 2410mV | 2310-2510mV |

使用 `espressif__button` v4.1.6 库，回调注册 API: `iot_button_register_cb()`
（旧版 v2/v3 的 `iot_button_set_evt_cb` / `iot_button_register_event_callback` 已废弃）。

### BMP 拍照流程 (PLAY 键, ~230KB)

```
camera_save_bmp_to_sdcard():
  1. 拿互斥锁 (阻塞等待)
  2. esp_camera_fb_get() → 获取 RGB565 帧
  3. 在内存中分配 BMP 缓冲区 (54+320×240×3 ≈ 230KB)
  4. 填充 BMP 文件头 + 像素转换 (RGB565 BE → BGR888, 底行在前)
  5. esp_camera_fb_return(fb) → sdio_write_binary_file()
  6. free(bmp) → 释放互斥锁
```

RGB565 是大端序 (与 ILI9341 一致)，不能直接用 `uint16_t*` 读取 (ESP32 小端序)，必须逐字节解析。

### JPEG 拍照流程 (SET 键, ~10-30KB)

```
camera_save_jpeg_to_sdcard():
  1. 拿互斥锁 (阻塞等待, app_main 自动跳过帧)
  2. esp_camera_deinit() → camera_init(JPEG)  (fb_count=1, GRAB_WHEN_EMPTY)
  3. 丢弃前 3 帧让 AWB/AEC/AGC 收敛 (否则画面偏绿)
  4. 抓取 JPEG 帧 (带重试 + SOI/EOI 完整性校验)
  5. sdio_write_binary_file() → esp_camera_fb_return(fb)
  6. esp_camera_deinit() → camera_init(RGB565)  恢复预览
  7. 释放互斥锁
```

JPEG 切换期间 LCD 短暂定格（约 200-400ms），锁定帧在切换回 RGB565 后恢复。

### 关键设计点

- **摄像头互斥锁**: `camera_capture` 非阻塞拿锁，JPEG 保存期间跳过帧；保存函数阻塞拿锁，确保独占
- **JPEG 白平衡**: 模式切换后丢弃 3 帧让 AWB 收敛，再抓正式帧
- **TCA9554 寄存器**: 输入端口 0x00, 输出端口 0x01, 极性反转 0x02, 配置端口 0x03。配置寄存器 0=输出 1=输入 (与直觉相反)
- **I2C 引脚**: SDA=GPIO17, SCL=GPIO18, 频率 100kHz
- **摄像头 SCCB**: 复用 TCA9554 的 I2C 总线 (I2C Port 0), `pin_sccb_sda=-1, pin_sccb_scl=-1`
- **RGB565 大端序**: camera_fb_t→buf 是 BE 字节序，逐字节解析，不能直接用 uint16_t*
- **JPEG 完整性校验**: 检查 SOI (0xFFD8) 和 EOI (0xFFD9) 标记，最多重试 3 次

### TCA9554 枚举约定

| 枚举类型 | 值 | 含义 |
|---|---|---|
| `esp_tca9554_io_level_t` | `TCA9554_IO_LOW=0`, `TCA9554_IO_HIGH=1` | 电平 |
| `esp_tca9554_io_config_t` | `TCA9554_IO_OUTPUT=0`, `TCA9554_IO_INPUT=1` | 引脚方向 |

### LCD 引脚映射

| 信号 | 引脚 | 控制方式 |
|------|------|----------|
| MOSI | IO0 | SPI 总线 |
| SCLK | IO1 | SPI 总线 |
| DC   | IO2 | 物理 GPIO (esp_lcd 自动) |
| CS   | TCA9554 P3 | I2C 扩展 (手动控制, cs_gpio_num=-1) |
| RST  | TCA9554 P2 | I2C 扩展 (手动复位, reset_gpio_num=-1) |
| BL   | TCA9554 P1 | I2C 扩展 (背光开关) |

### 摄像头 DVP 引脚映射

| 信号    | 引脚   |
|---------|--------|
| XCLK    | IO40   |
| PCLK    | IO11   |
| VSYNC   | IO21   |
| HREF    | IO38   |
| D0-D7   | IO13, 47, 14, 3, 12, 42, 41, 39 |
| PWDN    | -1 (未使用) |
| RESET   | -1 (未使用) |
| SIOD    | -1 (SCCB 复用 I2C) |
| SIOC    | -1 (SCCB 复用 I2C) |

### LCD 三层架构

- `espressif/esp_lcd_ili9341` — ILI9341 初始化命令序列
- `esp_lcd` (IDF SDK) — 型号无关的通用 panel API
- `lcd_board` — 把具体硬件（引脚、SPI、TCA9554）和上面两层接起来

### esp_lcd 通用 API

| 函数 | 用途 |
|------|------|
| `esp_lcd_panel_draw_bitmap()` | 绘制像素 |
| `esp_lcd_panel_swap_xy()` | XY 轴交换（旋转） |
| `esp_lcd_panel_mirror()` | 镜像翻转 |
| `esp_lcd_panel_invert_color()` | 颜色反转 |
| `esp_lcd_panel_disp_on_off()` | 开关显示 |
| `esp_lcd_panel_reset()` | 软件复位 |
| `esp_lcd_panel_del()` | 删除面板、释放资源 |

## 开发环境

- 本地 ESP-IDF 路径: `E:/App/Esp/esp-idf/frameworks/esp-idf-v5.5.3/`
- 工具链路径: `E:/App/Esp/esp-idf/`
- Python 路径: `E:\App\Esp\esp-idf\tools\idf-python\3.11.2\python.exe`
- 烧录端口: COM3 (UART)
- 支持 VS Code Dev Container (espressif/idf Docker 镜像)

## 添加新组件

### 本地组件

在 `components/` 下创建新目录，添加 `CMakeLists.txt` 使用 `idf_component_register` 注册源文件和依赖，然后在 `main/CMakeLists.txt` 的 `REQUIRES` 中添加该组件名。

### managed component

在 `idf_component.yml` 中声明依赖，`idf.py build` 首次运行时会自动下载到 `managed_components/`。

```yaml
dependencies:
  espressif/esp_lcd_ili9341: "*"
  espressif/esp32-camera: ^2.1.6
```

注意: `esp_lcd` 是 IDF SDK 自带的组件（与 `driver` 平级），使用其头文件时 CMakeLists.txt 的 `REQUIRES` 必须显式添加 `esp_lcd`。
