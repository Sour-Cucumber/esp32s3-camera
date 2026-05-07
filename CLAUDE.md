# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

Jacktter 是一个基于 ESP32-S3 的 ESP-IDF 项目，通过 I2C 总线驱动 TCA9554 GPIO 扩展芯片控制红蓝两颗 LED，同时通过 SPI 总线驱动 ILI9341 LCD 屏幕。

- **芯片**: ESP32-S3
- **框架**: ESP-IDF v5.5.3
- **I2C 扩展芯片**: TCA9554 (地址 0x20)
- **LCD 驱动芯片**: ILI9341 (SPI, 320×240)
- **构建系统**: CMake

## 构建与烧录

```bash
# 构建项目
idf.py build

# 烧录到设备 (COM3, UART)
idf.py -p COM3 flash

# 构建并烧录
idf.py -p COM3 build flash

# 监控串口输出
idf.py -p COM3 monitor

# 构建、烧录、监控一条龙
idf.py -p COM3 build flash monitor

# 清理构建产物
idf.py fullclean
```

## 架构

```
Jacktter/
├── main/                    # 主应用代码
│   ├── main.c               # 应用入口，LCD 初始化 + LED 闪烁任务
│   └── CMakeLists.txt       # 依赖 tca9554 + lcd_board 组件
├── components/              # 可复用的 ESP-IDF 组件
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h        # 驱动 API 头文件 (需 esp_bit_defs.h)
│   │   ├── tca9554.c        # 驱动实现
│   │   └── CMakeLists.txt   # 依赖 driver (I2C)
│   ├── ili9341/             # ILI9341 LCD 面板驱动 (模仿 ST7789 模式)
│   │   ├── ili9341.h        # esp_lcd_new_panel_ili9341() 声明
│   │   ├── ili9341.c        # 面板 vtable 实现 + 初始化序列
│   │   └── CMakeLists.txt   # 依赖 esp_lcd + driver
│   └── lcd_board/           # 板级整合层 (TCA9554 + SPI + ILI9341)
│       ├── lcd_board.h      # lcd_board_init() + lcd_board_set_backlight()
│       ├── lcd_board.c      # 板级初始化全链路
│       └── CMakeLists.txt   # 依赖 tca9554 + ili9341 + driver
├── .devcontainer/           # VS Code Dev Container (基于 espressif/idf 镜像)
├── CMakeLists.txt           # 项目根 CMake，引用 $ENV{IDF_PATH}/tools/cmake/project.cmake
└── sdkconfig                # ESP-IDF 项目配置
```

### 组件层次与数据流

```
main (app_main)
  ├── 依赖 tca9554 + lcd_board 组件
  ├── 初始化: tca9554_init() → lcd_board_init(&panel)
  ├── LCD: esp_lcd_panel_draw_bitmap() 等原生 API
  └── FreeRTOS 任务: red_led (100ms), blue_led (200ms)

components/lcd_board
  ├── 依赖 tca9554 + ili9341 + driver (SPI)
  ├── lcd_board_init(): 全链路初始化
  │   1. TCA9554 配控制引脚 (CS=P3, RST=P2, BL=P1)
  │   2. 硬件复位 LCD (通过 TCA9554)
  │   3. spi_bus_initialize(SPI2_HOST, IO0/IO1)
  │   4. esp_lcd_new_panel_io_spi(DC=IO2, CS=-1)
  │   5. esp_lcd_new_panel_ili9341(reset=-1)
  │   6. esp_lcd_panel_init() + disp_on_off(true)
  │   7. 背光开启
  └── lcd_board_set_backlight(percent): 0=灭, 1~100=亮

components/ili9341
  ├── 依赖 esp_lcd (框架) + driver (GPIO)
  ├── 实现 esp_lcd_panel_t vtable 接口
  └── panel_ili9341_init(): ILI9341 完整初始化序列 (电源/Gamma/MADCTL等)

components/tca9554
  ├── 依赖 ESP-IDF driver (I2C)
  ├── 私有函数: i2c_bus_init, tca9554_write_reg, tca9554_read_reg
  └── 公开 API:
      - tca9554_init()             — 初始化 I2C 总线
      - tca9554_set_io_config()    — 设置引脚方向 (输入/输出)
      - tca9554_get_io_config()    — 获取引脚方向
      - tca9554_set_output_level() — 设置输出引脚电平
      - tca9554_get_output_level() — 获取输出引脚电平
      - tca9554_get_input_level()  — 获取输入引脚电平
```

### 关键设计点

**TCA9554**
- 寄存器地址: 输入端口 0x00, 输出端口 0x01, 极性反转 0x02, 配置端口 0x03
- I2C 地址 0x20, 总线 SDA=GPIO17, SCL=GPIO18, 频率 100kHz
- 引脚配置使用位掩码: `BIT(n)` 宏来自 `esp_bit_defs.h`

**完整引脚映射**

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI MOSI (SDA) | IO0 | ILI9341 数据 |
| SPI SCLK (CLK) | IO1 | ILI9341 时钟 |
| DC | IO2 | ILI9341 命令/数据选择 |
| I2C SDA | IO17 | TCA9554 |
| I2C SCL | IO18 | TCA9554 |
| TCA9554 P1 | CTRL (BL) | 背光控制 |
| TCA9554 P2 | RST | LCD 硬件复位 |
| TCA9554 P3 | CS | SPI 片选 (常低) |
| TCA9554 P6 | LED 红色 | 闪烁 100ms 周期 |
| TCA9554 P7 | LED 蓝色 | 闪烁 200ms 周期 |

**ILI9341**
- SPI 总线: SPI2_HOST (FSPI), 时钟 40MHz, 模式 0
- 分辨率: 320×240, 颜色格式: RGB565
- 像素顺序: **RGB** (非 BGR)
- CS 由 TCA9554 P3 控制，esp_lcd 层设 `cs_gpio_num=-1` (常低)
- RST 由 TCA9554 P2 控制，esp_lcd 层设 `reset_gpio_num=-1` (手动复位)

**FreeRTOS 任务**: `red_led` (100ms) 和 `blue_led` (200ms) 各自独立运行在 Core 0

### TCA9554 枚举约定

| 枚举类型 | 值 | 含义 |
|---|---|---|
| `esp_tca9554_io_level_t` | `TCA9554_IO_LOW=0`, `TCA9554_IO_HIGH=1` | 电平 |
| `esp_tca9554_io_config_t` | `TCA9554_IO_OUTPUT=0`, `TCA9554_IO_INPUT=1` | 引脚方向 |

注意: 配置寄存器中 0=输出, 1=输入 (与直觉相反)。

## 开发环境

- 本地 ESP-IDF 路径: `E:/App/Esp/esp-idf/frameworks/esp-idf-v5.5.3/`
- 工具链路径: `E:/App/Esp/esp-idf/`
- Python 路径: `E:\App\Esp\esp-idf\tools\idf-python\3.11.2\python.exe`
- 烧录端口: COM3 (UART)
- 支持 VS Code Dev Container (基于 espressif/idf Docker 镜像，含 QEMU 支持)

## 添加新组件

在 `components/` 下创建新目录，添加 `CMakeLists.txt` 使用 `idf_component_register` 注册源文件和依赖，然后在 `main/CMakeLists.txt` 的 `REQUIRES` 中添加该组件名。
