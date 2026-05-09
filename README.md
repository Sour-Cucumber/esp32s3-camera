# Jacktter — ESP32-S3 + TCA9554 + ILI9341 LCD + OV2640 Camera

ESP32-S3 开发板项目，通过 I2C 驱动 TCA9554 GPIO 扩展芯片控制 ILI9341 LCD，并通过 DVP 接口驱动 OV2640 摄像头，实时采集图像并显示在 LCD 上 (320×240, RGB565)。

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
- **XCLK 频率**: 40MHz
- **像素格式**: RGB565 (与 LCD 一致)
- **帧大小**: QVGA (320×240)

## 功能

- TCA9554 I2C GPIO 扩展器管理 LCD 控制引脚 (CS/RST/BL)
- ILI9341 LCD 全屏实时显示 (320×240, 16bpp RGB565)
- OV2640 摄像头 DVP 采集，RGB565 格式，双缓冲 + 取最新帧
- `camera_capture()` 循环：采集 → 绘制到 LCD → 释放帧，约 100fps

## 构建与烧录

### 前置条件

- [ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/) 已安装并配置
- Windows 环境下请在 CMD 或 PowerShell 中运行 `idf.py`（**不支持 MSYS/MinGW**）

### 命令

```bash
# 构建
idf.py build

# 烧录 (COM3)
idf.py -p COM3 flash

# 串口监控
idf.py -p COM3 monitor

# 构建、烧录、监控一条龙
idf.py -p COM3 build flash monitor

# 清理
idf.py fullclean
```

## 工程结构

```
Jacktter/
├── main/                    # 应用入口 (TCA9554 → LCD → Camera → 采集循环)
│   ├── main.c
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h
│   │   ├── tca9554.c
│   │   └── CMakeLists.txt
│   ├── lcd_board/           # LCD 硬件初始化封装 (SPI + TCA9554 + ILI9341)
│   │   ├── lcd_board.h
│   │   ├── lcd_board.c
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml
│   └── camera_board/        # 摄像头硬件抽象层 (DVP + esp32-camera + LCD 绘制)
│       ├── camera_board.h
│       ├── camera_board.c
│       └── CMakeLists.txt
├── managed_components/      # idf.py build 自动下载
│   ├── espressif__esp_lcd_ili9341/
│   └── espressif__esp32-camera/
├── docs/
│   └── esp32-camera_API参考.md
├── .devcontainer/           # VS Code Dev Container
├── CMakeLists.txt
└── sdkconfig
```

## 组件架构

```
main (app_main)
  ├── tca9554       — I2C GPIO 扩展
  ├── lcd_board     — SPI LCD 初始化
  └── camera_board  — 摄像头初始化 + 帧采集/显示
        └── espressif/esp32-camera (managed component)

components/lcd_board
  ├── tca9554
  ├── esp_lcd (IDF SDK)
  └── espressif/esp_lcd_ili9341 (managed component)

components/camera_board
  ├── espressif/esp32-camera (managed component)
  └── lcd_board (通过 esp_lcd_panel_handle_t 写屏)
```

三层 LCD 驱动架构：
- `espressif/esp_lcd_ili9341` — 知道 ILI9341 初始化命令序列
- `esp_lcd` (IDF SDK) — 型号无关的通用 panel API
- `lcd_board` (本地组件) — 把具体硬件引脚和上面两层接起来

## TCA9554 API

| 函数 | 用途 |
|------|------|
| `tca9554_init()` | 初始化 I2C 总线 |
| `tca9554_set_io_config()` | 设置引脚方向 (0=输出, 1=输入) |
| `tca9554_get_io_config()` | 获取引脚方向 |
| `tca9554_set_output_level()` | 设置输出电平 |
| `tca9554_get_output_level()` | 获取输出电平 |
| `tca9554_get_input_level()` | 获取输入电平 |

## LCD Board API

| 函数 | 用途 |
|------|------|
| `lcd_board_init()` | 初始化硬件，返回 panel 句柄 |
| `lcd_board_set_backlight()` | 背光开关 |

## Camera Board API

| 函数 | 用途 |
|------|------|
| `camera_init()` | 初始化摄像头 (esp_camera_init) |
| `camera_capture()` | 采集一帧并绘制到 LCD |

## VS Code Dev Container

项目已配置 VS Code Dev Container（基于 `espressif/idf` 镜像，含 QEMU 支持），直接在 VS Code 中打开项目即可自动配置开发环境。
