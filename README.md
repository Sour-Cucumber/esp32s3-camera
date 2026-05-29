# Jacktter — ESP32-S3 + TCA9554 + ILI9341 LCD + OV2640 Camera

ESP32-S3-Korvo2 开发板项目，通过 I2C 驱动 TCA9554 GPIO 扩展芯片控制 ILI9341 LCD，DVP 接口驱动 OV2640 摄像头实时预览，ADC 按键拍照并保存到 SD 卡。

## 功能

- **LCD 实时预览**: ILI9341 全屏显示 (320×240, RGB565)，约 100fps
- **ADC 按键**: 6 个板载按键 (REC/MUTE/PLAY/SET/VOL-/VOL+)，单击触发拍照
- **BMP 拍照**: PLAY 键保存 BMP24 格式 (~230KB)，LCD 预览不中断
- **JPEG 拍照**: SET 键保存 JPEG 格式 (~10-30KB)，临时切换摄像头模式
- **SD 卡存储**: SDIO 模式写入
- **FreeRTOS 多任务**: 按键任务 + 采集循环独立运行，互斥锁保护摄像头共享资源

## 硬件连接

### I2C / LCD / 背光

| 信号 | 引脚 | 控制方式 |
|------|------|----------|
| I2C SDA | GPIO17 | ESP32-S3 内置 I2C |
| I2C SCL | GPIO18 | ESP32-S3 内置 I2C |
| SPI MOSI | IO0 | ESP32-S3 内置 SPI |
| SPI SCLK | IO1 | ESP32-S3 内置 SPI |
| LCD DC | IO2 | 物理 GPIO |
| LCD CS | TCA9554 P3 | I2C GPIO 扩展 |
| LCD RST | TCA9554 P2 | I2C GPIO 扩展 |
| LCD BL | TCA9554 P1 | I2C GPIO 扩展 |

- **TCA9554 I2C 地址**: 0x20
- **I2C 频率**: 100kHz

### 摄像头 DVP

| 信号  | 引脚   |
|-------|--------|
| XCLK  | IO40   |
| PCLK  | IO11   |
| VSYNC | IO21   |
| HREF  | IO38   |
| D0    | IO13   |
| D1    | IO47   |
| D2    | IO14   |
| D3    | IO3    |
| D4    | IO12   |
| D5    | IO42   |
| D6    | IO41   |
| D7    | IO39   |
| PWDN  | -1 (未使用) |
| RESET | -1 (未使用) |

- **摄像头**: OV2640
- **SCCB**: 复用 TCA9554 的 I2C 总线 (I2C Port 0)
- **XCLK 频率**: 20MHz
- **像素格式**: RGB565 (预览) / JPEG (拍照)
- **帧大小**: QVGA (320×240)

### ADC 按键 (ESP32-S3-Korvo2 板载)

| 按键 | 分压值 | ADC 通道 |
|------|--------|----------|
| REC | 380mV | ADC1 CH4 (GPIO5) |
| MUTE | 820mV | 同上 |
| PLAY | 1180mV | 同上 |
| SET | 1570mV | 同上 |
| VOL- | 1980mV | 同上 |
| VOL+ | 2410mV | 同上 |

6 个按键共享一根 ADC 线，通过分压电阻产生不同电压值来区分按键。

## 构建与烧录

### 前置条件

- [ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/) 已安装并配置
- Windows 环境下请在 CMD 或 PowerShell 中运行 `idf.py`（**不支持 MSYS/MinGW**）

### 命令

```bash
idf.py build                         # 构建
idf.py -p COM3 flash                 # 烧录
idf.py -p COM3 monitor               # 串口监控
idf.py -p COM3 build flash monitor   # 一条龙
idf.py fullclean                     # 清理
```

## 工程结构

```
Jacktter/
├── main/                    # 应用入口
│   ├── main.c               # 初始化 + 按键任务 + 采集循环
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   ├── lcd_board/           # LCD 硬件初始化封装 (SPI + TCA9554 + ILI9341)
│   ├── camera_board/        # 摄像头硬件抽象层 + BMP/JPEG 拍照
│   ├── sdcard/              # SD 卡 SDIO 驱动
│   └── board_buttons/       # 板载 6 键 ADC 按键封装 (espressif__button)
├── managed_components/      # idf.py build 自动下载
│   ├── espressif__esp_lcd_ili9341/
│   ├── espressif__esp32-camera/
│   └── espressif__button/
├── docs/
│   ├── esp32-camera_API参考.md
│   ├── espressif_button_guide.md
│   └── CLAUDE.md
├── .devcontainer/
├── CMakeLists.txt
└── sdkconfig
```

## 组件架构

```
main (app_main)
  ├── tca9554          — I2C GPIO 扩展
  ├── lcd_board        — SPI LCD 初始化
  ├── camera_board     — 摄像头初始化 + 帧采集/显示 + BMP/JPEG 拍照
  ├── sdcard           — SD 卡存储
  └── board_buttons    — ADC 按键 (REC/MUTE/PLAY/SET/VOL-/VOL+)
        └── espressif__button (managed component)

components/camera_board
  ├── espressif/esp32-camera
  ├── lcd_board (esp_lcd_panel_handle_t)
  └── sdcard (sdio_write_binary_file)

components/board_buttons
  └── espressif__button > button_adc
```

### 多任务架构

```
app_main (prio 1)               button_task (prio 10)
─────────────────               ─────────────────────
初始化所有外设                    阻塞在 xQueueReceive
while (1):                       │
  camera_capture() ←─ 互斥锁 ──→ │ 按键事件到达
    try-take mutex               │   camera_save_xxx_to_sdcard()
    采集帧 → 绘制 LCD              │     take mutex (阻塞等待)
    释放帧                         │     保存照片
    give mutex                   │     give mutex
  vTaskDelay(10ms)               │ 回到阻塞

按键回调 (esp_timer 上下文)
  └── xQueueSend → button_task
```

## Camera Board API

| 函数 | 用途 |
|------|------|
| `camera_init(format)` | 初始化摄像头 (RGB565 预览 或 JPEG 拍照) |
| `camera_capture(panel, x0, y0, x1, y1)` | 采集一帧绘制到 LCD（非阻塞互斥锁） |
| `camera_save_bmp_to_sdcard()` | 保存 BMP24 照片到 SD 卡（RGB565 模式，不中断预览） |
| `camera_save_jpeg_to_sdcard()` | 保存 JPEG 照片到 SD 卡（临时切换到 JPEG 模式） |

## Board Buttons API

| 函数 | 用途 |
|------|------|
| `board_buttons_init()` | 初始化 6 个 ADC 按键 |
| `board_button_get_handle(id)` | 获取指定按键句柄 |

按键编号枚举: `BOARD_BTN_REC`, `BOARD_BTN_MUTE`, `BOARD_BTN_PLAY`, `BOARD_BTN_SET`, `BOARD_BTN_VOL_DOWN`, `BOARD_BTN_VOL_UP`

## SD Card API

| 函数 | 用途 |
|------|------|
| `sdcard_init()` | 初始化 SD 卡（SDIO 模式） |
| `sdio_write_binary_file(path, buf, len)` | 写入二进制文件 |

## TCA9554 API

| 函数 | 用途 |
|------|------|
| `tca9554_init()` | 初始化 I2C 总线 |
| `tca9554_set_io_config()` | 设置引脚方向 (0=输出, 1=输入) |
| `tca9554_get_io_config()` | 获取引脚方向 |
| `tca9554_set_output_level()` | 设置输出电平 |
| `tca9554_get_output_level()` | 获取输出电平 |
| `tca9554_get_input_level()` | 获取输入电平 |

## VS Code Dev Container

项目已配置 VS Code Dev Container（基于 `espressif/idf` 镜像，含 QEMU 支持），直接在 VS Code 中打开项目即可自动配置开发环境。
