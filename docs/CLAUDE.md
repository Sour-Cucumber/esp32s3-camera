# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

esp32s3-camera 是一个基于 ESP32-S3 的 ESP-IDF 项目，通过 I2C 总线驱动 TCA9554 GPIO 扩展芯片控制 ILI9341 LCD，并通过 DVP 接口驱动 OV2640 摄像头模块，实时采集图像并显示在 LCD 上。

- **芯片**: ESP32-S3
- **框架**: ESP-IDF v5.5.3
- **I2C 扩展芯片**: TCA9554 (地址 0x20)
- **LCD 驱动**: ILI9341 (320×240, SPI, 16bpp RGB565)
- **LCD 驱动方案**: managed component `espressif/esp_lcd_ili9341`
- **摄像头**: OV2640 (DVP 并行接口, XCLK=40MHz, PIXFORMAT_RGB565, QVGA)
- **摄像头驱动方案**: managed component `espressif/esp32-camera` (^2.1.6)
- **构建系统**: CMake

## 构建与烧录

**重要**: ESP-IDF 不支持 MSYS/MinGW (git bash) 环境。必须在 Windows CMD 或 PowerShell 中运行 `idf.py`。

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
│   ├── main.c               # 应用入口: 初始化 TCA9554 → LCD → Camera → 采集循环
│   ├── CMakeLists.txt       # REQUIRES: tca9554 lcd_board camera_board esp32-camera
│   └── idf_component.yml    # 声明 espressif/esp_lcd_ili9341, espressif/esp32-camera
├── components/              # 可复用的 ESP-IDF 组件
│   ├── tca9554/             # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h        # 驱动 API 头文件 (需 esp_bit_defs.h)
│   │   ├── tca9554.c        # 驱动实现
│   │   └── CMakeLists.txt   # 依赖 driver (I2C)
│   ├── lcd_board/           # LCD 硬件初始化薄封装层
│   │   ├── lcd_board.h      # 公开 API（含 esp_lcd_panel_ops.h）
│   │   ├── lcd_board.c      # SPI + TCA9554 + ILI9341 初始化
│   │   ├── CMakeLists.txt   # REQUIRES: tca9554 driver esp_lcd esp_lcd_ili9341
│   │   └── idf_component.yml # 声明 espressif/esp_lcd_ili9341
│   └── camera_board/        # 摄像头硬件抽象层
│       ├── camera_board.h   # 公开 API + 引脚定义
│       ├── camera_board.c   # esp_camera 初始化 + 帧采集/显示
│       └── CMakeLists.txt   # REQUIRES: driver esp32-camera lcd_board
├── managed_components/      # idf.py build 自动下载
│   ├── espressif__esp_lcd_ili9341/
│   └── espressif__esp32-camera/
├── docs/
│   └── esp32-camera_API参考.md
├── .devcontainer/           # VS Code Dev Container (基于 espressif/idf 镜像)
├── CMakeLists.txt           # 项目根 CMake，引用 $ENV{IDF_PATH}/tools/cmake/project.cmake
└── sdkconfig                # ESP-IDF 项目配置
```

### 组件层次

```
main (app_main)
  ├── 依赖 tca9554 组件
  ├── 依赖 lcd_board 组件
  └── 依赖 camera_board 组件

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

components/lcd_board
  ├── 依赖 tca9554 (CS/RST/BL 引脚控制)
  ├── 依赖 esp_lcd (通用 LCD API: panel_ops, panel_io)
  ├── 依赖 espressif/esp_lcd_ili9341 (ILI9341 初始化序列)
  └── 公开 API:
      - lcd_board_init()            — 初始化硬件，返回 panel 句柄
      - lcd_board_set_backlight()   — 背光开关 (TCA9554 P1)

components/camera_board
  ├── 依赖 espressif/esp32-camera (OV2640 驱动)
  ├── 依赖 lcd_board (通过 esp_lcd_panel_handle_t 写屏)
  └── 公开 API:
      - camera_init()               — 初始化摄像头 (gpio + esp_camera_init)
      - camera_capture()            — 采集一帧并绘制到 LCD

managed_components/espressif__esp_lcd_ili9341
  ├── 通过 idf_component.yml 声明，idf.py build 自动下载
  └── 核心函数: esp_lcd_new_panel_ili9341()

managed_components/espressif__esp32-camera
  ├── 通过 idf_component.yml 声明，idf.py build 自动下载
  └── 核心函数: esp_camera_init(), esp_camera_fb_get(), esp_camera_fb_return()
```

### 应用主流程

```
app_main:
  1. tca9554_init()         — 初始化 I2C 总线
  2. lcd_board_init()       — 初始化 LCD (TCA9554 → 硬件复位 → SPI → Panel IO → Panel 设备)
  3. camera_init()          — 初始化摄像头 (esp_camera_init)
  4. while(1):
       camera_capture()     — 获取帧缓冲 → 绘制到 LCD → 释放帧缓冲
       vTaskDelay(10ms)
```

### 关键设计点

- **TCA9554 寄存器地址**: 输入端口 0x00, 输出端口 0x01, 极性反转 0x02, 配置端口 0x03
- **I2C 引脚**: SDA=GPIO17, SCL=GPIO18, 频率 100kHz
- **摄像头 SCCB**: 复用 TCA9554 的 I2C 总线 (I2C Port 0), `pin_sccb_sda=-1, pin_sccb_scl=-1`
- **摄像头帧缓冲**: fb_count=2 (双缓冲), grab_mode=CAMERA_GRAB_LATEST (取最新帧)
- **像素格式**: RGB565 (与 LCD 一致，无需转换)
- **引脚配置使用位掩码**: GPIO 编号通过 `BIT(n)` 定义为位掩码，`BIT` 宏来自 `esp_bit_defs.h`

### TCA9554 枚举约定

| 枚举类型 | 值 | 含义 |
|---|---|---|
| `esp_tca9554_io_level_t` | `TCA9554_IO_LOW=0`, `TCA9554_IO_HIGH=1` | 电平 |
| `esp_tca9554_io_config_t` | `TCA9554_IO_OUTPUT=0`, `TCA9554_IO_INPUT=1` | 引脚方向 |

注意: 配置寄存器中 0=输出, 1=输入 (与直觉相反)。

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

| 信号    | 引脚   | 说明           |
|---------|--------|----------------|
| XCLK    | IO40   | 主时钟 40MHz   |
| PCLK    | IO11   | 像素时钟       |
| VSYNC   | IO21   | 帧同步         |
| HREF    | IO38   | 行有效         |
| D0-D7   | IO13, 47, 14, 3, 12, 42, 41, 39 | 8 位并行数据 |
| PWDN    | -1     | 未使用         |
| RESET   | -1     | 未使用         |
| SIOD    | -1     | SCCB 复用 I2C  |
| SIOC    | -1     | SCCB 复用 I2C  |

### LCD 三层架构

- `espressif/esp_lcd_ili9341` (managed component) — 知道 ILI9341 的初始化命令序列
- `esp_lcd` (IDF SDK 自带) — 提供与型号无关的通用 panel API
- `lcd_board` (本地组件) — 把具体硬件（引脚、SPI、TCA9554）和上面两层接起来

main.c 只需 `#include "lcd_board.h"` 即可使用全部 LCD API（`esp_lcd_panel_ops.h` 已被包含）。

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
- 支持 VS Code Dev Container (基于 espressif/idf Docker 镜像，含 QEMU 支持)

## 添加新组件

### 本地组件

在 `components/` 下创建新目录，添加 `CMakeLists.txt` 使用 `idf_component_register` 注册源文件和依赖，然后在 `main/CMakeLists.txt` 的 `REQUIRES` 中添加该组件名。

### managed component (从 ESP-IDF 组件仓库引用)

在 `idf_component.yml` 中声明依赖，`idf.py build` 首次运行时会自动下载到 `managed_components/` 目录。

```yaml
dependencies:
  espressif/esp_lcd_ili9341: "*"
  espressif/esp32-camera: ^2.1.6
```

注意: `esp_lcd` 是 IDF SDK 自带的组件（与 `driver` 平级），使用其头文件时 CMakeLists.txt 的 `REQUIRES` 必须显式添加 `esp_lcd`，不能只靠 `driver`。
