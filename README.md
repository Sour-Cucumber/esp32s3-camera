# Jacktter — ESP32-S3 + TCA9554 + ILI9341 LCD Demo

ESP32-S3 开发板项目，通过 I2C 总线驱动 TCA9554 GPIO 扩展芯片（控制 LED 与 LCD 引脚），并使用 SPI 驱动 ILI9341 LCD 显示屏 (320×240, 16bpp RGB565)。

## 硬件连接

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
| LED 红 | TCA9554 P6 | I2C GPIO 扩展 |
| LED 蓝 | TCA9554 P7 | I2C GPIO 扩展 |

- **TCA9554 I2C 地址**: 0x20
- **I2C 频率**: 100kHz

## 功能

- FreeRTOS 双任务 LED 闪烁：红灯 100ms 周期，蓝灯 200ms 周期
- LCD 全屏颜色循环测试（红、绿、蓝、白、黑，每 2 秒切换）

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
├── main/                    # 应用入口 (LED 闪烁 + LCD 测试任务)
│   ├── main.c
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h
│   │   ├── tca9554.c
│   │   └── CMakeLists.txt
│   └── lcd_board/           # LCD 硬件初始化封装 (SPI + TCA9554 + ILI9341)
│       ├── lcd_board.h
│       ├── lcd_board.c
│       ├── CMakeLists.txt
│       └── idf_component.yml
├── managed_components/      # idf.py build 自动下载
│   └── espressif__esp_lcd_ili9341/
├── docs/
│   └── lcd_architecture.md
├── .devcontainer/           # VS Code Dev Container
├── CMakeLists.txt
└── sdkconfig
```

## 组件架构

```
main (app_main)
  ├── tca9554  — I2C GPIO 扩展
  └── lcd_board — SPI LCD 初始化
        ├── tca9554
        ├── esp_lcd (IDF SDK)
        └── espressif/esp_lcd_ili9341 (managed component)
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

## VS Code Dev Container

项目已配置 VS Code Dev Container（基于 `espressif/idf` 镜像，含 QEMU 支持），直接在 VS Code 中打开项目即可自动配置开发环境。
