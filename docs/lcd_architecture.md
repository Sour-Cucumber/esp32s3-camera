# LCD 驱动架构说明

## 概览

```
┌─────────────────────────────────────────────────────────────────┐
│                      你的 main.c                                │
│                                                                 │
│  lcd_board_init(&panel)  ────→ 拿到 panel 句柄                   │
│  esp_lcd_panel_draw_bitmap(panel, ...)  ────→ 用句柄画图         │
│  esp_lcd_panel_mirror(panel, ...)       ────→ 用句柄镜像         │
│  lcd_board_set_backlight(true)          ────→ 背光控制           │
└──────────────────────────┬──────────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
    ① lcd_board      ② esp_lcd       ③ esp_lcd_ili9341
    (本地组件)       (IDF SDK组件)    (managed component)
```

## 三个组件，各司其职

### ① `lcd_board` — 硬件初始化层

| 项目 | 内容 |
|------|------|
| 位置 | `components/lcd_board/` |
| 类型 | 本地组件 |
| 依赖 | `tca9554`, `driver`, `esp_lcd`, `espressif/esp_lcd_ili9341` |

**职责**：知道你的硬件是怎么接线的，把所有底层硬件准备好，最终产出一个 `panel` 句柄。

初始化流程：

```
lcd_board_init()
│
├─ 1. 配置 TCA9554 引脚为输出（CS=P3, RST=P2, BL=P1）
├─ 2. 硬件复位：RST 拉低 10ms → 拉高 120ms
├─ 3. 初始化 SPI 总线（MOSI=IO0, SCLK=IO1, 40MHz）
├─ 4. 创建 SPI panel IO 句柄（DC=IO2, CS 由 TCA9554 手动控制）
├─ 5. CS 拉低（选中 LCD）
├─ 6. 调用 esp_lcd_new_panel_ili9341() → 创建 ILI9341 面板句柄
├─ 7. 调用 esp_lcd_panel_reset() / init() / disp_on_off()
└─ 8. 背光点亮
```

公开 API：

| 函数 | 用途 |
|------|------|
| `lcd_board_init(esp_lcd_panel_handle_t *out_panel)` | 初始化 LCD，返回 panel 句柄 |
| `lcd_board_set_backlight(bool on)` | 背光开关 |

### ② `esp_lcd` — 通用 LCD 操作库

| 项目 | 内容 |
|------|------|
| 位置 | ESP-IDF SDK `components/esp_lcd/` |
| 类型 | IDF SDK 自带组件 |
| 头文件 | `esp_lcd_panel_ops.h`, `esp_lcd_types.h`, `esp_lcd_panel_io.h` |

**职责**：提供一套与具体 LCD 型号无关的通用 API。不管你是 ILI9341、ST7789 还是 NT35510，拿到 panel 句柄后都用同一套函数操作。

可用 API：

| 函数 | 用途 |
|------|------|
| `esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, data)` | 绘制像素 |
| `esp_lcd_panel_swap_xy(panel, bool)` | XY 轴交换（旋转 90°/270°） |
| `esp_lcd_panel_mirror(panel, x, y)` | 镜像翻转 |
| `esp_lcd_panel_invert_color(panel, bool)` | 颜色反转 |
| `esp_lcd_panel_disp_on_off(panel, bool)` | 开关显示 |
| `esp_lcd_panel_disp_sleep(panel, bool)` | 休眠模式 |
| `esp_lcd_panel_set_gap(panel, x, y)` | 设置显示偏移 |
| `esp_lcd_panel_reset(panel)` | 软件复位 |
| `esp_lcd_panel_init(panel)` | 初始化 |
| `esp_lcd_panel_del(panel)` | 删除面板、释放资源 |

> main.c 只需 `#include "lcd_board.h"`，该头文件已包含 `esp_lcd_panel_ops.h`，以上 API 全部可用。

### ③ `espressif/esp_lcd_ili9341` — ILI9341 专用驱动

| 项目 | 内容 |
|------|------|
| 来源 | ESP-IDF 组件仓库（`idf_component.yml` 声明，`idf.py build` 自动下载） |
| 下载位置 | `managed_components/espressif__esp_lcd_ili9341/` |
| 头文件 | `esp_lcd_ili9341.h` |
| 版本约束 | `"*"`（任意版本） |

**职责**：知道 ILI9341 这个型号的初始化命令序列（几十条寄存器的配置值），并提供 `esp_lcd_new_panel_ili9341()` 把 ILI9341 注册到 `esp_lcd` 通用框架里。

核心函数：

```c
esp_err_t esp_lcd_new_panel_ili9341(
    const esp_lcd_panel_io_handle_t io,         // SPI IO 句柄
    const esp_lcd_panel_dev_config_t *config,   // 面板配置
    esp_lcd_panel_handle_t *ret_panel           // 输出：panel 句柄
);
```

## 硬件引脚映射

| 信号 | 引脚 | 控制方式 |
|------|------|----------|
| MOSI | IO0 | SPI 总线 |
| SCLK | IO1 | SPI 总线 |
| DC | IO2 | 物理 GPIO（esp_lcd 自动控制） |
| CS | TCA9554 P3 | I2C GPIO 扩展（lcd_board 手动控制） |
| RST | TCA9554 P2 | I2C GPIO 扩展（lcd_board 手动复位） |
| BL | TCA9554 P1 | I2C GPIO 扩展（背光开关） |

> CS、RST、BL 不走物理 GPIO 是因为都挂在 TCA9554 I2C 扩展芯片上。

## 依赖引用链

```
main/CMakeLists.txt
  └─ REQUIRES: tca9554  lcd_board

components/lcd_board/CMakeLists.txt
  └─ REQUIRES: tca9554  driver  esp_lcd

components/lcd_board/idf_component.yml
  └─ dependencies: espressif/esp_lcd_ili9341: "*"
         │
         └─ idf.py build 时自动下载到 managed_components/
```

## 项目文件结构

```
Jacktter/
├── main/
│   ├── main.c                  # 应用入口：LED 任务 + LCD 测试任务
│   ├── CMakeLists.txt          # 依赖 tca9554、lcd_board
│   └── idf_component.yml       # 声明 espressif/esp_lcd_ili9341 依赖
├── components/
│   ├── tca9554/                # TCA9554 I2C GPIO 扩展器驱动
│   │   ├── tca9554.h
│   │   ├── tca9554.c
│   │   └── CMakeLists.txt
│   └── lcd_board/              # LCD 硬件初始化层（薄封装）
│       ├── lcd_board.h         # 公开 API + 包含 esp_lcd_panel_ops.h
│       ├── lcd_board.c         # SPI 初始化 + TCA9554 控制 + ILI9341 面板创建
│       ├── CMakeLists.txt      # 依赖 tca9554、driver、esp_lcd
│       └── idf_component.yml   # 声明 espressif/esp_lcd_ili9341 依赖
├── managed_components/         # idf.py build 自动生成
│   └── espressif__esp_lcd_ili9341/
│       └── include/
│           └── esp_lcd_ili9341.h
└── docs/
    └── lcd_architecture.md     # 本文档
```

## 如何更换其他 LCD 型号

比如换成 ST7789：

1. 修改 `components/lcd_board/idf_component.yml`：`espressif/esp_lcd_st7789: "*"`
2. 修改 `lcd_board.c`：把 `esp_lcd_new_panel_ili9341()` 换成 `esp_lcd_new_panel_st7789()`
3. 修改 `lcd_board.h` 和 `.c` 中的分辨率宏（240×240 等）
4. 如果需要改引脚，修改 `lcd_board.c` 中的 `#define LCD_*_GPIO`

`esp_lcd` 的通用 API 完全不用变，`main.c` 无需改动。
