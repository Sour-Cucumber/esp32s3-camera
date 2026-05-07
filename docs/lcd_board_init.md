# ILI9341 LCD 初始化详解

## 引脚分配

| 信号 | 引脚 | 类型 | 说明 |
|---|---|---|---|
| MOSI | GPIO 0 | ESP32 直连 | SPI 数据输出，高速信号 |
| SCLK | GPIO 1 | ESP32 直连 | SPI 时钟，40MHz |
| D/C | GPIO 2 | ESP32 直连 | 命令/数据选择，高速翻转 |
| BL | TCA9554 P1 | I2C GPIO 扩展 | 背光开关，低速信号 |
| RST | TCA9554 P2 | I2C GPIO 扩展 | 硬件复位，低速信号 |
| CS | TCA9554 P3 | I2C GPIO 扩展 | 片选，低速信号 |

**设计决策**: BL、RST、CS 三根线走 TCA9554（I2C GPIO 扩展器），共享 I2C 总线仅需 2 条物理线，节省 ESP32 的 GPIO 引脚。这三根线均为低速信号（电平翻转频率低、延迟不敏感），经 I2C 转接完全够用。MOSI、SCLK、D/C 是 40MHz 的高速信号，必须走 ESP32 直连 GPIO。

---

## 初始化流程

```
TCA9554 引脚配置 → 硬件复位 → SPI 总线初始化
  → Panel IO 创建 → 手工拉低 CS → Panel 设备创建
    → 软件复位 → 出厂初始化序列 → 开显示 → 开背光 → 完成
```

### 第 1 步：配置 TCA9554 引脚方向与初始电平

```c
tca9554_set_io_config(LCD_BL_TCAGPIO, TCA9554_IO_OUTPUT);  // P1 → 输出
tca9554_set_io_config(LCD_RST_TCAGPIO, TCA9554_IO_OUTPUT); // P2 → 输出
tca9554_set_io_config(LCD_CS_TCAGPIO, TCA9554_IO_OUTPUT);  // P3 → 输出

tca9554_set_output_level(LCD_CS_TCAGPIO, TCA9554_IO_HIGH);  // CS 拉高，取消片选
tca9554_set_output_level(LCD_RST_TCAGPIO, TCA9554_IO_LOW);  // RST 拉低，进入复位状态
tca9554_set_output_level(LCD_BL_TCAGPIO, TCA9554_IO_LOW);   // 背光关闭
```

TCA9554 的配置寄存器中 **0 = 输出、1 = 输入**（与直觉相反）。CS 初始为高（未选中），在 SPI 总线初始化完成前避免总线上的杂散信号误写入 ILI9341。

---

### 第 2 步：ILI9341 硬件复位

```c
vTaskDelay(pdMS_TO_TICKS(10));   // RST 保持低电平 ≥10ms
tca9554_set_output_level(LCD_RST_TCAGPIO, TCA9554_IO_HIGH);  // 释放复位
vTaskDelay(pdMS_TO_TICKS(120));  // 等待 120ms 让内部电源稳定
```

ILI9341 数据手册要求：
- 复位脉冲低电平至少 **10µs**（代码取了更安全的 10ms）
- 复位释放后需要 **120ms** 等待内部稳压器、振荡器起振

---

### 第 3 步：SPI 总线初始化

```c
spi_bus_config_t bus_cfg = {
    .mosi_io_num = 0,
    .miso_io_num = -1,
    .sclk_io_num = 1,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 320 * 240 * 2 + 8,
};
spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
```

| 字段 | 值 | 原因 |
|---|---|---|
| `mosi_io_num` | 0 | ESP32-S3 任意 GPIO 均可作 SPI 引脚 |
| `miso_io_num` | -1 | ILI9341 虽支持读回，但本设计只写不读，禁用以节省资源 |
| `quadwp / quadhd` | -1 | 不使用 QSPI，标准 4 线 SPI 即可 |
| `max_transfer_sz` | `320×240×2+8` | 一帧 RGB565 数据 (320×240×2 = 153,600 字节) + 8 字节指令/参数开销，确保单次 DMA 能传输完整帧 |

**关于 `+8`**: `320 × 240 × 2` 是一帧完整像素数据。一次 SPI 事务除了像素数据还会在前面附加指令字节（如 `0x2C` 内存写入）。多加 8 字节确保 DMA 缓冲区能容纳协议开销，防止缓冲区溢出。

---

### 第 4 步：创建 Panel IO（SPI 传输协议层）

```c
esp_lcd_panel_io_spi_config_t io_cfg = {
    .cs_gpio_num = -1,
    .dc_gpio_num = 2,
    .spi_mode = 0,
    .pclk_hz = 40 * 1000 * 1000,
    .trans_queue_depth = 10,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
};
esp_lcd_new_panel_io_spi(SPI2_HOST, &io_cfg, &io_handle);
```

| 字段 | 值 | 原因 |
|---|---|---|
| `cs_gpio_num` | -1 | CS 在 TCA9554（I2C GPIO 扩展器）上，ESP-IDF SPI 驱动无法直接控制，设为 -1 告知驱动"CS 我手动管理" |
| `dc_gpio_num` | 2 | D/C 是高速信号（每字节需翻转一次），必须用 ESP32 直连 GPIO。驱动自动管理：发命令时拉低、发数据时拉高 |
| `spi_mode` | 0 | CPOL=0（时钟空闲为低）、CPHA=0（第一个边沿采样），符合 ILI9341 时序要求 |
| `pclk_hz` | 40MHz | `pclk_hz` 是 **SPI 外设时钟频率**（不是帧率）。它决定了数据传输速度上限。ILI9341 官方写周期最小 66ns（≈15MHz），但实测 40MHz 可稳定运行 |
| `trans_queue_depth` | 10 | 最多 10 个 SPI 事务排队，防止生产者（应用层）过快淹没队列 |
| `lcd_cmd_bits` | 8 | ILI9341 所有命令均为单字节（8 位）。对应 8080 并行接口转 SPI 的约定 |
| `lcd_param_bits` | 8 | 所有参数均为单字节（8 位） |

---

### 第 5 步：手工片选 ILI9341

```c
tca9554_set_output_level(LCD_CS_TCAGPIO, TCA9554_IO_LOW);
```

前一步 `cs_gpio_num = -1` 告知驱动不管理 CS，因此必须手动拉低 CS 选中芯片。此后所有 SPI 命令/数据都经由这个已选中的 ILI9341。

---

### 第 6 步：创建 ILI9341 Panel 设备

```c
esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = -1,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
};
esp_lcd_new_panel_ili9341(io_handle, &panel_cfg, &panel);
```

| 字段 | 值 | 原因 |
|---|---|---|
| `reset_gpio_num` | -1 | 硬件复位已在第 2 步通过 TCA9554 完成，不需要驱动再复位一次 |
| `rgb_ele_order` | RGB | ILI9341 默认像素字节序中 R 在最高位、G 在中间、B 在最低位 |
| `bits_per_pixel` | 16 | RGB565 格式：R=5bit, G=6bit, B=5bit，每像素 2 字节，ILI9341 原生支持。比 RGB888 省一半内存 |

---

### 第 7 步：软件复位 → 初始化序列 → 开显示 → 开背光

```c
esp_lcd_panel_reset(panel);              // 软件复位命令 0x01
esp_lcd_panel_init(panel);               // 发送出厂初始化序列
esp_lcd_panel_disp_on_off(panel, true);  // Display ON 命令 0x29
lcd_board_set_backlight(true);           // TCA9554 P1 输出高 → 背光亮
```

| 调用 | 发送的命令 | 作用 |
|---|---|---|
| `panel_reset` | `0x01` (SWRESET) | 即使硬件已复位，软复位确保内部状态机回到已知初始态 |
| `panel_init` | `0x11` (SLPOUT), `0x3A` (COLMOD), `0x36` (MADCTL), 伽马曲线等 | 唤醒屏幕、设置像素格式为 16bpp、配置伽马校正、设置扫描方向等。这些命令均由 ESP-IDF 内置 ILI9341 驱动自动发送 |
| `disp_on_off(true)` | `0x29` (DISPON) | 打开显示，帧存储器内容开始刷新到玻璃面板 |
| `set_backlight(true)` | TCA9554 P1 拉高 | 点亮背光 LED |

---

## 关键概念

### pclk_hz ≠ 帧率

`pclk_hz` 是 SPI 外设时钟频率（40MHz），它决定 **数据传输速度的上限**，而非实际帧率。

```
每帧数据量 = 320 × 240 × 2 = 153,600 字节
理论极限帧率 = 40,000,000 / (153,600 × 8) ≈ 32.5 fps
实际帧率更低（指令开销、DMA 延迟、应用层处理时间）
```

### 为什么 CS 和 RST 不交给驱动管理

ESP-IDF 的 LCD 驱动可以直接管理直连 GPIO 上的 CS 和 RST 引脚。但本设计中它们通过 I2C 总线连接 TCA9554 GPIO 扩展器，驱动无法跨总线控制，因此设为 `-1` 后手动管理。

### SPI Mode 0

- **CPOL=0**: 时钟空闲时 SCLK 为低电平
- **CPHA=0**: 数据在第一个时钟边沿（上升沿）被采样
- ILI9341 数据手册要求在 SCLK 上升沿采样数据，因此 Mode 0 和 Mode 3 都可用，Mode 0 更常见
