# espressif__button 库 API 使用指南

## 库版本

4.1.6 (managed_components/espressif__button)

## 1. GPIO 按键

适用于单个物理按键，每个按键占用一个独立 GPIO。

### 1.1 配置结构体

```c
#include "iot_button.h"
#include "button_gpio.h"

// 按键行为参数（0 = 使用 Kconfig 默认值）
button_config_t btn_cfg = {
    .long_press_time = 0,   // 长按触发时间 (ms)，默认 1500
    .short_press_time = 0,  // 短按判定时间 (ms)，默认 180
};

// GPIO 硬件参数
button_gpio_config_t gpio_cfg = {
    .gpio_num = 0,            // GPIO 引脚号
    .active_level = 0,        // 0=低电平有效(启用内部上拉), 1=高电平有效(启用内部下拉)
    .enable_power_save = false, // 是否启用 light-sleep 唤醒
    .disable_pull = false,    // 是否禁用内部上下拉
};
```

### 1.2 初始化流程

```c
button_handle_t btn = NULL;
esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);
// 可重复调用创建多个按键
```

### 1.3 释放

```c
iot_button_delete(btn);
```

---

## 2. ADC 按键

适用于多个按键共享一根 ADC 通道，通过分压电阻产生不同电压值来区分按键。

### 2.1 ESP32-S3 ADC 硬件基础

| 硬件单元 | 通道数 | 限制 |
|---------|--------|------|
| ADC1 (ADC_UNIT_1) | 10 个 (CH0~CH9) | 无限制（首选） |
| ADC2 (ADC_UNIT_2) | 10 个 (CH0~CH9) | 与 Wi-Fi 共享，Wi-Fi 开启时不可用 |

**关键点**：`unit_id + adc_channel` 决定了 GPIO 引脚，这是芯片硬件的固定映射，不需要在配置中显式指定 GPIO。

| Channel | GPIO |
|---------|------|
| CH0 | GPIO1 |
| CH1 | GPIO2 |
| CH2 | GPIO3 |
| CH3 | GPIO4 |
| CH4 | GPIO5 |
| CH5 | GPIO6 |
| CH6 | GPIO7 |
| CH7 | GPIO8 |
| CH8 | GPIO9 |
| CH9 | GPIO10 |

### 2.2 配置结构体

```c
#include "iot_button.h"
#include "button_adc.h"

// 按键行为参数（同 GPIO）
button_config_t btn_cfg = {0};

// ADC 硬件参数
button_adc_config_t adc_cfg = {
    .adc_handle = NULL,       // 设为 NULL，库内部自动创建 ADC 句柄
    .unit_id = ADC_UNIT_1,    // 使用 ADC1（不受 Wi-Fi 限制）
    .adc_channel = 4,         // GPIO5（硬件固定映射）
    .button_index = 0,        // 该通道上第几个按键 (0~7)，软件编号
    .min = 190,               // 电压窗口下限 (mV)
    .max = 600,               // 电压窗口上限 (mV)
};
```

### 2.3 电压窗口的计算方式

6 个按键共享一根 ADC 线，按下时产生不同分压值 `vol[n]`。库通过比较 ADC 采样值落在 `[min, max]` 窗口来识别是哪个按键：

```
按键0: min = (0 + vol[0]) / 2                 max = (vol[0] + vol[1]) / 2
按键i: min = (vol[i-1] + vol[i]) / 2          max = (vol[i] + vol[i+1]) / 2
末尾 : min = (vol[n-2] + vol[n-1]) / 2        max = (vol[n-1] + 3000) / 2
```

应用示例 (ESP32-S3-Korvo2, 6 按键)：

```c
const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};

for (size_t i = 0; i < 6; i++) {
    adc_cfg.button_index = i;
    if (i == 0) {
        adc_cfg.min = (0 + vol[i]) / 2;
    } else {
        adc_cfg.min = (vol[i - 1] + vol[i]) / 2;
    }
    if (i == 5) {
        adc_cfg.max = (vol[i] + 3000) / 2;
    } else {
        adc_cfg.max = (vol[i] + vol[i + 1]) / 2;
    }
    iot_button_new_adc_device(&btn_cfg, &adc_cfg, &btns[i]);
}
```

### 2.4 原理图模型

```
GPIO5 (ADC1_CH4) ─── 上拉电阻 ─── VDD_3.3V  （好像电压正好反了）
       │
       ├── 按键0 ── R1 ── GND   →  按下时电压 = 380mV
       ├── 按键1 ── R2 ── GND   →  按下时电压 = 820mV
       ├── 按键2 ── R3 ── GND   →  按下时电压 = 1180mV
       ...
       └── 按键N ── Rn ── GND   →  按下时电压 = Vn mV
```

### 2.5 释放

```c
iot_button_delete(btns[i]);  // 逐个释放
```

---

## 3. 事件注册

### 3.1 所有可用事件

| 事件 | 触发时机 |
|------|---------|
| `BUTTON_PRESS_DOWN` | 按键按下 |
| `BUTTON_PRESS_UP` | 按键释放 |
| `BUTTON_PRESS_REPEAT` | 连击的每次额外按下 |
| `BUTTON_PRESS_REPEAT_DONE` | 连击序列全部结束 |
| `BUTTON_SINGLE_CLICK` | 单击 |
| `BUTTON_DOUBLE_CLICK` | 双击 |
| `BUTTON_MULTIPLE_CLICK` | 多击（需指定点击次数） |
| `BUTTON_LONG_PRESS_START` | 长按达到阈值 |
| `BUTTON_LONG_PRESS_HOLD` | 长按期间周期性触发 |
| `BUTTON_LONG_PRESS_UP` | 长按后释放 |
| `BUTTON_PRESS_END` | 任意按键序列完全结束 |

### 3.2 事件状态机流程

```
按下  →  BUTTON_PRESS_DOWN
         ├─ 短按后释放 → BUTTON_PRESS_UP
         │    ├─ 单击 → BUTTON_SINGLE_CLICK
         │    ├─ 再次按下(超时前) → BUTTON_PRESS_REPEAT (多次)
         │    │    └─ 全部结束 → BUTTON_PRESS_REPEAT_DONE
         │    │         ├─ 双击 → BUTTON_DOUBLE_CLICK
         │    │         └─ N击 → BUTTON_MULTIPLE_CLICK (需注册指定 N)
         │    └─ BUTTON_PRESS_END
         │
         └─ 按住超过 long_press_time
              ├─ BUTTON_LONG_PRESS_START (到达阈值时一次)
              ├─ BUTTON_LONG_PRESS_HOLD (周期性反复触发)
              └─ 释放 → BUTTON_LONG_PRESS_UP → BUTTON_PRESS_UP → BUTTON_PRESS_END
```

### 3.3 回调函数签名

```c
typedef void (*button_cb_t)(void *button_handle, void *usr_data);
```

### 3.4 注册普通事件（不需要额外参数）

```c
iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, my_callback, NULL);
iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, my_callback, NULL);
iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, my_callback, NULL);
iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, my_callback, NULL);
iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, NULL, my_callback, NULL);
iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, NULL, my_callback, NULL);
```

### 3.5 注册多击事件（需指定点击次数）

```c
button_event_args_t args = { .multiple_clicks.clicks = 3 };
iot_button_register_cb(btn, BUTTON_MULTIPLE_CLICK, &args, my_callback, NULL);
```

### 3.6 注册特定时长的长按

```c
button_event_args_t args = { .long_press.press_time = 2000 };
iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, &args, my_callback, NULL);
// 上例：只在按住 2000ms 时触发一次，不响应其他时长的长按
```

### 3.7 注销回调

```c
iot_button_unregister_cb(btn, BUTTON_PRESS_DOWN, NULL);  // 注销该事件所有回调
```

### 3.8 同一事件注册多个回调

```c
iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, cbA, NULL);
iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, cbB, NULL);
// cbA 和 cbB 都会在单击时触发，按注册顺序执行
```

---

## 4. 运行时查询

```c
button_event_t event = iot_button_get_event(btn);       // 当前事件
const char *str = iot_button_get_event_str(event);      // 事件名称字符串
uint8_t level = iot_button_get_key_level(btn);          // 1=按下 0=释放
uint32_t dur = iot_button_get_pressed_time(btn);        // 已按下毫秒数
uint8_t repeat = iot_button_get_repeat(btn);            // 连击次数
uint16_t cnt = iot_button_get_long_press_hold_cnt(btn); // 长按 hold 触发次数
```

---

## 5. 运行时修改参数

```c
uint16_t t = 2000;
iot_button_set_param(btn, BUTTON_LONG_PRESS_TIME_MS, &t);
iot_button_set_param(btn, BUTTON_SHORT_PRESS_TIME_MS, &t);
```

---

## 6. Kconfig 配置项

| 符号 | 默认值 | 范围 | 含义 |
|------|--------|------|------|
| `BUTTON_PERIOD_TIME_MS` | 5 | 2-500 | 按键扫描周期，越小越灵敏但 CPU 开销越大 |
| `BUTTON_DEBOUNCE_TICKS` | 2 | 1-7 | 消抖计数值，实际消抖时间 = PERIOD_TIME × DEBOUNCE_TICKS |
| `BUTTON_SHORT_PRESS_TIME_MS` | 180 | 50-800 | 短按判定时间 |
| `BUTTON_LONG_PRESS_TIME_MS` | 1500 | 500-5000 | 长按触发阈值 |
| `BUTTON_LONG_PRESS_HOLD_SERIAL_TIME_MS` | 20 | 2-1000 | 长按 hold 重复触发间隔 |
| `ADC_BUTTON_MAX_CHANNEL` | 3 | 1-5 | ADC 最大通道数 |
| `ADC_BUTTON_MAX_BUTTON_PER_CHANNEL` | 8 | 1-10 | 每通道最大按键数 |
| `ADC_BUTTON_SAMPLE_TIMES` | 1 | 1-4 | ADC 每次扫描采样次数 |

---

## 7. CMakeLists.txt 配置

```cmake
# main/CMakeLists.txt
idf_component_register(SRCS "main.c"
                       REQUIRES espressif__button)
```

---

## 8. 注意事项

1. **回调上下文**：所有回调在 `esp_timer` 任务上下文中执行，不要做耗时操作或调用可能阻塞的 API。
2. **ADC2 限制**：如果项目使用 Wi-Fi，必须用 `ADC_UNIT_1`，不能使用 `ADC_UNIT_2`。
3. **多按键共存**：多个按键共享一个全局扫描定时器，库内部用链表管理，无需担心冲突。
4. **低电平有效** (active_level=0) 需要外部上拉或启用内部上拉；**高电平有效** (active_level=1) 需要外部下拉或启用内部下拉。
5. `button_adc_config_t.button_index` 是纯软件编号，与 GPIO 无关，只用来区分同一通道上的不同分压按键。
