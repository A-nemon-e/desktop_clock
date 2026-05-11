# ESP32-S3 Desktop Clock — Full Stack Work Plan

## TL;DR

> **Quick Summary**: 构建一款ESP32-S3桌面时钟，48×16白光LED矩阵（6×IS31FL3729通过TCA9548A），含WiFi/IP定位/NTP/卡片显示/OTA/后端管理/天气系统。架构为ESP-IDF v5.5固件 + FastAPI后端 + Vue3+ElementPlus前端 + PostgreSQL + Mosquitto。
>
> **Deliverables**: ESP32-S3固件 · FastAPI后端 · Vue3前端 · PostgreSQL schema · Mosquitto配置
>
> **Estimated Effort**: XL | **Parallel**: YES — 14 waves, max 8 concurrent | **Tasks**: 48+4

---

## Context

### Original Request
用户自制硬件：48×16白光LED矩阵（6×IS31FL3729 · TCA9548A I2C mux · AHT21 · 按键 · ESP32-S3 N16R8）。要求桌面时钟含WiFi/属地查询/NTP/卡片显示系统/胖瘦模式/OTA/后端管理/天气。开发环境 ESP-IDF v5.5。

### Decisions Confirmed
- **技术栈**: FastAPI + Vue3/ElementPlus + PostgreSQL + Mosquitto + ESP-IDF v5.5
- **认证**: Session + MAC白名单 · **胖瘦模式**: 自动切换 · **测试**: Unity(fw)+pytest(be)
- **显示效果**: 参考开源移植 · **音频**: Phase2 · **部署**: x64 Linux VPS · **仅做Rev0.1**

### Hardware Confirmations (Updated)
- **TCA9548A RESET**: 已接上拉电阻 → 不需要GPIO控制, 固件无需处理RESET
- **RTC电池**: 无 → 每次断电需完整WiFi→IP→TZ→NTP流程(约30s)
- **Rev 0.2**: 暂不考虑, 仅开发Rev 0.1 (带TCA9548A)
- **功耗**: USB供电, 固件中MAX_GCC_VALUE做硬件保护

### CRITICAL: Display-First Architecture
> 除WiFi连接外，**任何任务不得阻塞显示刷新**。display_task=最高FreeRTOS优先级(5)。所有I2C/网络/MQTT/OTA使用独立任务，通过Queue通信。

### Button Behavior (Confirmed)
| 操作 | 动作 | 细节 |
|---|---|---|
| 单击 | 开关屏幕 | toggle display_task |
| 双击 | 切换卡片 | card_next(), 环形 |
| 长按 | 调节亮度 | 循环增/减(GCC 0-127)，到头停止，松手反向 |

---

## Work Objectives

### Core Objective
构建完整桌面时钟系统：固件驱动LED显示→WiFi→IP定位→时区→NTP→卡片系统→MQTT同步→OTA升级 + 后端管理面板 + 前端卡片编辑器。

### Must Have
- 显示 ≥30fps，任何操作不影响显示
- WiFi凭证持久化，多次失败自动切AP模式
- IP→时区→NTP全自动，双API备份
- 胖/瘦自动切换
- 设备配置重启不丢失
- OTA带自动回滚

### Must NOT Have
- 显示被网络操作阻塞（除WiFi连接）
- OTA中断变砖
- 卡片超9个
- 未绑定设备报错
- 天气同地区15min内重复查询

---

## Verification Strategy

| 决策 | 值 |
|---|---|
| 固件测试 | Unity (TDD) |
| 后端测试 | pytest (TDD) |
| QA方式 | Agent-executed（Playwright/curl/idf.py monitor） |
| 证据目录 | `.sisyphus/evidence/` |

---

## Execution Waves

```
Wave 0: HW Validation (3 tasks) → Wave 1: Foundation (4) → Wave 2: Drivers (5)
→ Wave 3: Display Engine (8) → Wave 4: Network (4) → Wave 5: Geo+NTP (2)
→ Wave 6: MQTT+Cards (4) → Wave 7: OTA (2) → Wave 8: Backend Foundation (4)
→ Wave 9: Device API (4) → Wave 10: Card+OTA API (4) → Wave 11: Frontend (3)
→ Wave 12: Integration (4) → Wave FINAL: Review (4)
```

---

## TODOs

---- [x] 0.1 **电源预算测量**

  **What to do**:
  ```c
  // 理论最大: 768 LEDs × 20mA × 3.3V ≈ 50W
  // 实际测量: 万用表测USB 5V输入电流
  // 全灭 → 记录I_min;  50%亮度 → 记录I_mid;  全亮(GCC=127, PWM=255) → 记录I_max
  // 计算安全GCC上限: GCC_max = 127 * (I_rated / I_max)
  // 写入头文件: #define MAX_GCC_VALUE <safe_value>
  ```
  确认USB供电能力（通常5V/2A=10W），设置GCC上限保证不超电源额定值。

  **Must NOT do**: 未确认电源能力前设置高GCC值（可能烧USB口或电源）

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 0 (with 0.2, 0.3)
  **Commit**: `chore(hw): document power budget and set MAX_GCC_VALUE`

  **Acceptance**:
  - [ ] 记录I_min/I_mid/I_max
  - [ ] MAX_GCC_VALUE已定义，附计算说明

- [x] 0.2 **I2C信号完整性验证**

  **What to do**:
  ```c
  // 1. 确认上拉电阻: SCL/SDA → 3.3V 应有 2.2k-4.7kΩ
  // 2. 示波器观察400kHz波形: 上升沿斜率、过冲、振铃
  // 3. 扫描I2C总线所有地址:
  //    主线(0x38=AHT21, 0x70=TCA9548A)
  //    mux通道0-5: 各应有一个3729地址
  // 4. TCA9548A RESET: 已硬件上拉, 无需固件控制, 仅需确认
  ```
  信号质量差则降速至100kHz并记录。

  **Must NOT do**: 信号质量差强行上400kHz

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 0 (with 0.1, 0.3)
  **Commit**: `chore(hw): I2C bus scan results and signal integrity report`

  **Acceptance**:
  - [ ] I2C地址扫描输出所有8个设备（1 AHT21 + 1 Mux + 6 3729）
  - [ ] RESET状态已确认，如需GPIO已分配
  - [ ] I2C频率选择已记录（400kHz/100kHz）并附理由

- [x] 0.3 **引脚连接确认**

  **What to do**:
  ```c
  // 编写GPIO测试固件: 逐个引脚输出HIGH→万用表验证→输出LOW→验证
  // 验证清单:
  //   IO1=SCL, IO2=SDA → I2C总线
  //   IO8=SDB → 所有3729的SDB（确认是并联一个GPIO控全部）
  //   IO4=LED → 指示灯
  //   IO38=Button → 按通3.3V, 上升沿中断
  //   IO15=I2S_WS, IO16=I2S_CLK, IO18=I2S_MICDATA
  //   IO19=D-, IO20=D+ → USB
  // 生成: components/config/pinmap.h
  //   #define PIN_SDB GPIO_NUM_8
  //   #define PIN_LED GPIO_NUM_4
  //   #define PIN_BUTTON GPIO_NUM_38
  //   ... etc
  ```

  **Must NOT do**: 假设引脚正确就开始写驱动代码

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 0 (with 0.1, 0.2)
  **Commit**: `chore(hw): create pinmap.h with verified pin assignments`

  **QA Scenarios**:
  ```
  Scenario: SDB功能验证
    Tool: Bash(idf.py flash monitor)
    Steps:
      1. 固件初始化后拉高SDB → I2C扫描记录可见设备
      2. 拉低SDB → I2C扫描确认3729全部消失
    Expected: SDB=HIGH时6个3729可见, SDB=LOW时不可见
    Evidence: .sisyphus/evidence/task-0.3-sdb.txt
  ```

- [x] 1. **ESP-IDF项目创建 + 分区表 + sdkconfig**

  **What to do**:
  1. `idf.py create-project clock_fw` 创建项目
  2. 创建 `partitions.csv`:
  ```csv
  nvs,      data, nvs,      0x9000,   0x6000,
  otadata,  data, ota,      0xf000,   0x2000,
  phy_init, data, phy,      0x11000,  0x1000,
  factory,  app,  factory,  0x20000,  0x300000,
  ota_0,    app,  ota_0,    ,         0x300000,
  ota_1,    app,  ota_1,    ,         0x300000,
  nvs_user, data, nvs,      ,         0x6000,
  storage,  data, spiffs,   ,         0x200000,
  ```
  3. `sdkconfig.defaults`:
  ```ini
  CONFIG_ESP32S3_SPIRAM_SUPPORT=y
  CONFIG_SPIRAM_USE_CAPS_ALLOC=y
  CONFIG_PARTITION_TABLE_CUSTOM=y
  CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
  CONFIG_ESP_HTTPS_OTA=y
  CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
  ```
  4. 创建components/目录 + 各组件CMakeLists.txt骨架

  **Must NOT do**: factory分区<2MB; 忘开PSRAM

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 1 (with 2, 3, 4)
  **Commit**: `feat: ESP-IDF project with OTA partition table and PSRAM config`

  **QA**: `idf.py build` → 成功; `idf.py partition-table` → 含factory/ota_0/ota_1

- [x] 2. **FreeRTOS任务架构 + 帧缓冲结构**

  **What to do**:
  ```c
  // === main/task_prio.h ===
  #define PRIO_DISPLAY    5   // 最高: I2C刷新(30fps)
  #define PRIO_RENDER     4   // 背景/前景渲染
  #define PRIO_WIFI       3
  #define PRIO_MQTT       3
  #define PRIO_BUTTON     3
  #define PRIO_NETWORK    2   // IP/时区/NTP(一次性)
  #define PRIO_OTA        2
  #define PRIO_THIN_HTTP  1   // 最低: 瘦模式HTTP server

  // === components/display/include/framebuffer.h ===
  typedef struct {
      uint8_t pixels[16][48];  // [row][col], 0-255 grayscale
  } framebuffer_t;

  // 双缓冲
  extern framebuffer_t fb_front;    // display_task正在刷新
  extern framebuffer_t fb_back;     // render_task正在渲染
  extern SemaphoreHandle_t fb_mutex; // 交换时短暂锁定

  void fb_swap(void);  // 交换front/back + 信号量保护
  ```
  **任务本质**:
  - `display_task`: 每33ms: fb_swap() → 遍历6通道→写3729 PWM→触发更新
  - `render_task`: 等待事件→渲染背景+前景→写入back buffer→通知display_task
  - 所有网络/MQTT任务通过Queue发送数据，不在display_task中处理

  **Must NOT do**: 非display_task直接操作I2C/3729; 高优先级忙等

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 1 (with 1, 3, 4)
  **Commit**: `feat: FreeRTOS task architecture with Display-First priority scheme`

  **Acceptance**:
  - [ ] task_prio.h已定义所有优先级
  - [ ] framebuffer.h含双缓冲结构+swap函数签名

- [x] 3. **全局类型 + 配置结构体 + 错误码**

  **What to do**:
  ```c
  // === components/shared/include/types.h ===
  typedef enum {
      BG_NONE = -1,
      BG_FIRE = 0,
      BG_CODE_RAIN,
      BG_WATER_RIPPLE,
      BG_GAME_OF_LIFE,
      BG_HOURGLASS,
      BG_PONG_CLOCK,
      BG_WEATHER,
      BG_COUNT
  } bg_type_t;

  typedef enum {
      FG_CLOCK_HMS = 0,     // "HH:MM:SS"
      FG_CLOCK_HM,          // "HH:MM"
      FG_DATE_WEEK_FULL,    // "YYYY MM DD\nDayname"
      FG_DATE_WEEK_COMPACT, // "MM DD DAY"
      FG_TEMP_HUMID,        // "Hum：XX%\nTemp： XX.X C" (中文冒号，Temp后有空格)
      FG_COUNT
  } fg_type_t;

  typedef enum {
      FONT_3x5 = 0,
      FONT_5x5,
      FONT_7x9,
      FONT_LONG,     // "更长"字体: 宽度可变, 高度11+像素, 用于大号时钟数字
      FONT_COUNT
  } font_size_t;

  // FONT_LONG 说明: 用于时钟前景的窄长数字字体
  //   数字宽度: 2-3px (比3×5更窄更高)
  //   高度: 11-14px (充分利用16px屏高)
  //   间距: 1px (紧凑排列)
  //   适用于仅显示时钟数字(HH:MM:SS或HH:MM)的场景
  //   字符集: '0'-'9', ':', ' '

  // === components/shared/include/errors.h ===
  typedef enum {
      ERR_OK = 0,
      ERR_WIFI_TIMEOUT,
      ERR_WIFI_AUTH_FAIL,
      ERR_MQTT_DISCONNECTED,
      ERR_HTTP_TIMEOUT,
      ERR_OTA_VERIFY_FAIL,
      ERR_OTA_ROLLBACK,
      // ... etc
  } error_code_t;
  ```

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 1 (with 1, 2, 4)
  **Commit**: `feat: global types, enums, and error codes`

- [x] 4. **日志系统 + 构建系统**

  **What to do**:
  ```c
  // components/log/include/log.h
  #define LOG_TAG_DISPLAY  "DSP"
  #define LOG_TAG_WIFI     "WIFI"
  #define LOG_TAG_MQTT     "MQTT"
  #define LOG_TAG_GEO      "GEO"
  // 封装 ESP_LOGI/ESP_LOGE, 所有模块统一格式
  ```
  根 `CMakeLists.txt`: 设置 `EXTRA_COMPONENT_DIRS` 指向 `components/`。
  每个component的CMakeLists.txt: `idf_component_register(SRCS ... INCLUDE_DIRS include REQUIRES driver esp_http_client ...)`

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 1 (with 1, 2, 3)
  **Commit**: `feat: logging macros and component build system`

---- [x] 5. **IS31FL3729 LED矩阵驱动**

  **What to do**:
  ```c
  // components/led_matrix/is31fl3729.h
  // ⚠️ 寄存器地址参考 QMK 生产驱动验证:
  #define IS31FL3729_ADDR_GND     0x34  // AD0=AD1=GND (NOT 0x60!)
  #define IS31FL3729_REG_PWM      0x01  // PWM基址 0x01-0x8E (142 LEDs, 16×8=128 used)
  #define IS31FL3729_REG_SCALING  0x90  // Scaling寄存器 0x90-0x9F (可选)
  #define IS31FL3729_REG_CONFIG   0xA0  // 配置寄存器 (NOT 0x02!)
  #define IS31FL3729_REG_GCC      0xA1  // Global Current Control (NOT 0x00!)
  #define IS31FL3729_REG_RESET    0xCF  // 软复位

  // 关键: IS31FL3729 没有独立的 UPDATE 寄存器!
  // 写操作通过I2C auto-increment连续写入PWM寄存器,
  // 写完所有PWM值后硬件自动锁存。

  // 初始化序列 (per driver):
  void is31fl3729_init(uint8_t addr);
  //   1. 拉高SDB (IO8, 所有3729并联) + delay(1ms)
  //   2. 写REG_CONFIG = 0x01 (normal operation mode)
  //   3. 写REG_GCC = 0x7F (最大亮度, 后续可调)
  //   4. 可选: 写REG_PWM_FREQUENCY (如需要)
  //   5. 写所有PWM寄存器 = 0x00 (全灭) — burst write from 0x01

  // 刷新一帧 (per driver, 处理8列×16行 = 128 LEDs):
  void is31fl3729_update(uint8_t addr, const uint8_t *pwm_data);
  //   pwm_data[128]: 每个LED一个8-bit PWM值
  //   1. I2C写: START → addr → REG_PWM(0x01) → pwm_data[0..127] → STOP
  //      利用I2C auto-increment, 一次burst写入全部PWM值
  //   2. 硬件在STOP后自动锁存新PWM值

  // GCC动态调节:
  void is31fl3729_set_gcc(uint8_t addr, uint8_t gcc); // 0xA1 ← gcc
  ```
  **TDD先写test**: 使用Unity mock I2C → 验证init序列写入的寄存器地址和值。
  必须对照IS31FL3729 datasheet验证所有寄存器地址。

  **Must NOT do**: ISR中执行I2C; 每帧重写CONFIG; 使用错误的寄存器地址

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 2 (with 6, 7, 8, 9) | **Blocks**: Task 9
  **Commit**: `feat(led): IS31FL3729 register-level driver (verified against QMK source)`

  **Acceptance**:
  - [ ] Unity test: `test_init_writes_correct_registers` → 验证REG_CONFIG=0xA0←0x01, REG_GCC=0xA1←0x7F
  - [ ] Unity test: `test_update_burst_writes_pwm` → 验证从0x01开始的burst write
  - [ ] 烧录: 全屏点亮→全灭→灰度渐变正常

  **QA Scenarios**:
  ```
  Scenario: 全屏255→0→渐变
    Tool: idf.py flash monitor + 拍照
    Steps:
      1. 所有PWM=255 → 拍照全白
      2. 所有PWM=0 → 拍照全黑
      3. 列0→列47 依次灰度递增 → 验证平滑
    Expected: 全白均匀, 全黑无光, 渐变连续
    Evidence: .sisyphus/evidence/task-5-brightness.png
  ```

- [x] 6. **TCA9548A I2C多路复用器驱动**

  **What to do**:
  ```c
  // components/i2c_mux/tca9548a.h
  #define TCA9548A_ADDR 0x70  // A0=A1=A2=GND

  void tca9548a_init(void);
  //   RESET已硬件上拉, 无需固件操作
  //   写控制寄存器=0x00 (关闭所有通道)

  void tca9548a_select(uint8_t ch);  // 0-7
  //   1. 写 (1 << ch) 到控制寄存器
  //   2. I2C STOP (esp-idf自动处理)
  //   3. 通道激活, 后续I2C操作直接发给3729 (地址0x34)

  void tca9548a_disable_all(void);   // 写0x00
  ```
  注意: AHT21在主线(0x38), 不经过mux。mux切换不影响AHT21通信。
  3729地址是0x34 (AD0=AD1=GND), 不是0x60!

  **Must NOT do**: 未发STOP就访问mux通道设备

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 2 (with 5, 7, 8, 9)
  **Commit**: `feat(i2c): TCA9548A mux channel selection driver`

  **Acceptance**: 通道0-5各发现1个3729地址; disable后无3729地址
  **QA Evidence**: `.sisyphus/evidence/task-6-mux-scan.txt`

- [x] 7. **AHT21温湿度传感器驱动**

  **What to do**:
  ```c
  // components/sensors/aht21.h
  #define AHT21_ADDR 0x38

  esp_err_t aht21_init(void);
  //   1. I2C写 [0xBA] (软复位) → delay(20ms)
  //   2. I2C写 [0xE1, 0x08, 0x00] (校准) → delay(300ms)
  //   3. 读状态字节确认 bit[3]=1(校准完成)

  esp_err_t aht21_read(float *temp, float *humid);
  //   1. I2C写 [0xAC, 0x33, 0x00] (触发测量)
  //   2. delay(75ms)
  //   3. I2C读 6字节: [status, hum_h, hum_l, hum_low4+temp_high4, temp_l, temp_low4]
  //   4. 解析: raw_h = (hum_h<<12) | (hum_l<<4) | (hum_low4>>4)
  //            humidity = (raw_h / 1048576.0) * 100
  //            raw_t = ((temp_high4&0x0F)<<16) | (temp_l<<8) | temp_low4
  //            temperature = (raw_t / 1048576.0) * 200 - 50
  ```
  **TDD**: Unity mock I2C，给定已知raw bytes→验证解析结果正确。

  **Must NOT do**: display_task中阻塞75ms; 每1秒以上才读一次

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 2 (with 5, 6, 8, 9)
  **Commit**: `feat(sensor): AHT21 temperature/humidity driver`

  **Acceptance**: 室内读数在合理范围(Hum:20-80%, Temp:15-35°C), 连续3次稳定
  **QA Evidence**: `.sisyphus/evidence/task-7-temp-humid.txt`

- [x] 8. **按键驱动（单击/双击/长按）**

  **What to do**:
  ```c
  // components/button/button.h
  typedef enum {
      BTN_NONE = 0,
      BTN_SINGLE_CLICK,
      BTN_DOUBLE_CLICK,
      BTN_LONG_PRESS_START,   // 开始长按
      BTN_LONG_PRESS_HOLD,    // 持续长按(每200ms触发一次, 用于亮度调节)
      BTN_LONG_PRESS_RELEASE, // 松手
  } button_event_t;

  void button_init(void);
  //   1. gpio_config IO38 = INPUT, 上升沿中断
  //   2. gpio_install_isr_service + gpio_isr_handler_add
  //   3. 创建Queue: btn_event_queue

  // ISR中: 记录时间戳 → xQueueSendFromISR()
  // button_task中:
  void button_task(void *pv);
  //   1. 收到ISR消息 → 记录press_time
  //   2. 等待释放(电平变低) → 计算duration
  //   3. duration < 300ms: 等300ms看有无第二次按下
  //      - 有(间隔<500ms) → BTN_DOUBLE_CLICK
  //      - 无 → BTN_SINGLE_CLICK
  //   4. duration > 800ms → BTN_LONG_PRESS_START
  //      - 每200ms → BTN_LONG_PRESS_HOLD
  //      - 松手 → BTN_LONG_PRESS_RELEASE
  ```

  **Must NOT do**: ISR中复杂逻辑(只记时间戳); 阻塞ISR等消抖

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 2 (with 5, 6, 7, 9)
  **Commit**: `feat(button): GPIO interrupt button with click/double/long-press detection`

  **Acceptance**: 4种事件均正确识别(日志可验证)
  **QA Evidence**: `.sisyphus/evidence/task-8-button-events.txt`

- [x] 9. **帧缓冲 + 显示刷新任务**

  **What to do**:
  ```c
  // components/display/display_task.c

  void display_task(void *pv) {
      TickType_t last_wake = xTaskGetTickCount();
      while (1) {
          vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(33)); // 30fps

          // 1. 交换缓冲
          if (xSemaphoreTake(fb_mutex, pdMS_TO_TICKS(5))) {
              swap(fb_front, fb_back);
              xSemaphoreGive(fb_mutex);
          }

          // 2. 遍历6个mux通道
          for (int ch = 0; ch < 6; ch++) {
              tca9548a_select(ch);
              // 每个3729负责8列×16行 = 128 LEDs
              // 准备128字节PWM数据 (按3729的LED映射排列)
              uint8_t pwm_data[128];
              int col_offset = ch * 8;
              // 按行优先或列优先映射到pwm_data[] (取决于3729 matrix wiring)
              for (int led = 0; led < 128; led++) {
                  int row = led / 8;   // 0-15
                  int col = led % 8;   // 0-7
                  pwm_data[led] = fb_front.pixels[row][col_offset + col];
              }
              // Burst write: 从REG_PWM(0x01)开始连续写128字节
              is31fl3729_update(ADDR_FOR_CH(ch), pwm_data);
          }
          tca9548a_disable_all();
      }
  }

  // 给渲染任务的接口:
  framebuffer_t *fb_get_back(void); // 获取back buffer引用(渲染用)
  void fb_mark_dirty(void);          // 标记back已更新
  void fb_set_gcc_all(uint8_t gcc);  // 全局亮度(长按调节)
  ```

  **Must NOT do**: display_task中任何网络/sensor操作; 动态内存分配

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 2 (after 5,6) | **Blocks**: Tasks 10-17
  **Commit**: `feat(display): 30fps double-buffered display refresh task`

  **Acceptance**:
  - [ ] 刷新周期<10ms (30fps有余量)
  - [ ] WiFi扫描时帧率不受影响
  - [ ] fb_swap无撕裂

  **QA Scenarios**:
  ```
  Scenario: 30fps稳定性(含网络负载)
    Tool: idf.py flash monitor
    Steps:
      1. 启动display_task, 同时运行WiFi扫描
      2. 记录100帧的间隔 → min/max/avg
    Expected: avg < 35ms, max < 50ms
    Evidence: .sisyphus/evidence/task-9-framerate.txt
  ```

---- [x] 10. **字体渲染器（3×5 / 5×5 / 7×9）**

  **What to do**:
  ```c
  // components/display/fonts/font_3x5.h
  // 每个字符: 5列 × 3位高 → 15 bits = 2 bytes
  // 例: '0' = {0b111, 0b101, 0b101, 0b101, 0b111}
  //         '1' = {0b010, 0b110, 0b010, 0b010, 0b111}
  //   char_map: '0'-'9', 'A'-'Z', ':', ' ', '-', '/', '°', '%', '.'
  //
  // font_5x5.h: 5×5 = 25 bits = 4 bytes per char. 同上字符集.
  //
  // font_7x9.h: 7×9 = 63 bits = 8 bytes per char.
  //   额外字符: MON, TUE, WED, THU, FRI, SAT, SUN (缩写)

  // 通用渲染函数:
  void render_char(framebuffer_t *fb, int x, int y,
                   char ch, font_size_t font, uint8_t brightness);
  //   x,y = 左上角像素坐标
  //   遍历font bitmap → 将亮bit写入 fb->pixels[y+dy][x+dx] = brightness

  void render_text(framebuffer_t *fb, int x, int y,
                   const char *text, font_size_t font, uint8_t brightness);
  //   逐字符调用render_char, 自动计算间距(x += font_w + 1)
  ```

  **Must NOT do**: 字体数据占用DRAM(全部`const PROGMEM`); 动态分配

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 11-17)
  **Commit**: `feat(font): 3x5, 5x5, 7x9 bitmap font renderer`

  **QA**: 3种字体分别渲染 "88:88" → 拍照验证清晰可辨
  **Evidence**: `.sisyphus/evidence/task-10-fonts.png`

- [x] 11. **火焰背景效果**

  **What to do**:
  ```c
  // components/display/effects/bg_fire.c
  // 参考 Doom Fire 算法, 适配48×16 灰度

  static uint8_t fire[16][48]; // 高度场(0=冷/黑, 255=热/白)

  void bg_fire_init(void);
  //   底部行(row 15)随机值 128-255 (火焰底部最热)
  //   其余行 = 0

  void bg_fire_update(void);
  //   for row = 0..14 (从下往上第二行开始):
  //     for col = 0..47:
  //       // 取下方3像素平均, 减衰减
  //       avg = (fire[row+1][col] +
  //              fire[row+1][max(0,col-1)] +
  //              fire[row+1][min(47,col+1)]) / 3;
  //       decay = random(0, 8);  // 随机衰减
  //       fire[row][col] = (avg > decay) ? avg - decay : 0;
  //   底部行: fire[15][col] = random(64, 255); // 新火源

  void bg_fire_render(framebuffer_t *fb);
  //   复制fire[][] → fb->pixels[][] (已经是灰度值)
  ```
  **约束**: 火焰背景只能搭配3×5字体前景(因火焰分辨率低)。

  **Must NOT do**: 浮点运算(全部整数)

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 10, 12-17)
  **Commit**: `feat(effect): Doom Fire background with integer propagation`

  **QA**: 运行10秒火焰动画 → 判断30fps流畅无卡顿 → 拍照/短视频
  **Evidence**: `.sisyphus/evidence/task-11-fire.mp4`

- [x] 12. **代码雨背景效果**

  **What to do**:
  ```c
  // components/display/effects/bg_code_rain.c
  #define MAX_DROPS 24  // 最多24个雨滴列

  typedef struct {
      int8_t col;       // 列位置 0-47
      int8_t row;       // 头部当前位置 -16..15
      int8_t speed;     // 下落速度 1-3
      uint8_t length;   // 尾巴长度 3-8
  } raindrop_t;

  static raindrop_t drops[MAX_DROPS];
  static char chars[48][16];  // 屏幕字符(随机Katakana/ASCII)

  void bg_coderain_init(void);
  //   随机初始化~16个雨滴, 生成随机字符表

  void bg_coderain_update(void);
  //   每帧:
  //   - 雨滴头部row += speed; 如超出屏幕→重置到顶部随机列
  //   - 随机新雨滴(概率5%/帧)

  void bg_coderain_render(framebuffer_t *fb, rect_t *mask, int mask_count);
  //   对每个雨滴:
  //   - 头部: 最亮(255)
  //   - 尾部: 渐变(255 → 200 → 150 → 100 → 50 → 0)
  //   - 跳过mask中的矩形(前景区域不落雨)
  ```
  **约束**: 代码雨必须提供mask_rect让前景区域不下雨。

  **Must NOT do**: mask区域有雨滴穿透

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 10, 11, 13-17)
  **Commit**: `feat(effect): Matrix code rain with foreground mask`

  **QA**: 设置mask=(10,2,28,12) → 该区域无雨滴, 外正常 → 拍照
  **Evidence**: `.sisyphus/evidence/task-12-rain-mask.png`

- [x] 13. **水波纹背景效果**

  **What to do**:
  ```c
  // components/display/effects/bg_water.c
  // 2D水波模拟 (48×16)

  static int16_t height[2][16][48]; // 双缓冲高度场(定点数, 缩放256)
  static uint8_t buf_idx = 0;       // 当前读取的buffer索引

  void bg_water_init(void);
  //   全部清零

  void bg_water_update(void);
  //   int cur = buf_idx, nxt = 1 - buf_idx;
  //   for row=1..14, col=1..46:
  //     h_sum = height[cur][row-1][col] + height[cur][row+1][col]
  //           + height[cur][row][col-1] + height[cur][row][col+1];
  //     height[nxt][row][col] = (h_sum / 2) - height[nxt][row][col];
  //     height[nxt][row][col] -= height[nxt][row][col] >> 6; // 阻尼1/64
  //   buf_idx = nxt;
  //   随机雨滴(概率10%/帧): height[nxt][rand_y][rand_x] += 512;

  void bg_water_render(framebuffer_t *fb);
  //   每像素: brightness = 128 + (height[cur][row][col] >> 4);
  //   clamp到0..255范围
  ```
  **约束**: 水波纹不能和任何前景组合(全屏显示)。

  **Must NOT do**: 浮点运算

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 10-12, 14-17)
  **Commit**: `feat(effect): 2D water ripple simulation with random drops`

  **QA**: 运行15秒观察涟漪 → 拍照/短视频
  **Evidence**: `.sisyphus/evidence/task-13-water.mp4`

---- [x] 14. **生命游戏背景效果**

  **What to do**:
  ```c
  // components/display/effects/bg_life.c
  // Conway's Game of Life — 48×16 网格

  static uint8_t grid[2][16][48]; // 双缓冲: 0=死, 1=活
  static uint8_t cur = 0;
  static uint8_t age[16][48];     // 存续时间(用于灰度渐变)

  void bg_life_init(uint32_t seed);
  //   随机~30%密度初始化, age=0

  void bg_life_update(void); // 每3帧(10代/秒)调用一次
  //   int nxt = 1-cur;
  //   for row=0..15, col=0..47:
  //     neighbors = 8方向计数
  //     if grid[cur][row][col]: (存活)
  //       grid[nxt][row][col] = (neighbors==2 || neighbors==3);
  //     else: (死亡)
  //       grid[nxt][row][col] = (neighbors==3); // 新生
  //     if grid[nxt][row][col]: age[row][col]++; else: age[row][col]=0;
  //   cur = nxt;

  void bg_life_render(framebuffer_t *fb);
  //   活细胞: brightness = 128 + (age[row][col] % 128);
  //           新生的暗, 存续久的亮 → 视觉层次
  //   死细胞: 0
  ```
  **约束**: 生命游戏不能和前景组合(全屏显示)。边界无环绕(边缘外=死)。

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 10-13, 15-17)
  **Commit**: `feat(effect): Conway's Game of Life with age-based grayscale`

  **QA**: seed=42运行30秒 → 截图第0代和第100代 → 验证B3/S23规则
  **Evidence**: `.sisyphus/evidence/task-14-life-gen0.png, task-14-life-gen100.png`

- [x] 15. **沙漏背景效果**

  **What to do**:
  ```c
  // components/display/effects/bg_hourglass.c
  // 简单粒子沙漏动画

  typedef struct { float x, y; } sand_particle_t; // 固定点位置(可换成定点数)
  static sand_particle_t particles[50];
  static uint8_t sand[16][48]; // 累积沙位

  void bg_hourglass_init(void);
  //   画沙漏轮廓到fb(三角形+颈+下三角+边框)
  //   粒子随机分布在上半三角区

  void bg_hourglass_update(void);
  //   每帧:
  //   - 粒子 y += 0.3 (重力)
  //   - 粒子 x += random(-0.2, 0.2) (横向漂移)
  //   - 碰到沙漏壁→反弹或停留在壁上
  //   - 通过颈部时x缩小范围(瓶颈)
  //   - 到达底部→停止并更新sand[][]
  //   - 上半粒子耗尽→翻转沙漏(所有粒子重新初始到上半)

  void bg_hourglass_render(framebuffer_t *fb);
  //   画沙漏轮廓(亮线, brightness=200)
  //   画粒子(brightness=255, 单像素)
  //   画累积沙(sand[][], brightness=150)
  ```
  **约束**: 沙漏不能和前景组合。

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (with 10-14, 16, 17)
  **Commit**: `feat(effect): hourglass particle animation`

  **QA**: 运行30秒观察粒子从上半流向下半 → 拍照
  **Evidence**: `.sisyphus/evidence/task-15-hourglass.png`

- [x] 16. **Pong时钟背景 + 专用前景**

  **What to do**:
  ```c
  // components/display/effects/bg_pong.c
  // Pong游戏 + 时钟叠加

  // Pong状态:
  static int ball_x, ball_y, ball_dx, ball_dy;
  static int paddle_left_y, paddle_right_y;

  void bg_pong_update(void);
  //   球移动: x+=dx, y+=dy; 碰壁反弹; 碰球拍反弹
  //   球拍位置: left=小时*1.3映射到0-16, right=分钟*0.266映射到0-16

  void bg_pong_render(framebuffer_t *fb, pong_fg_config_t *cfg);
  //   画球拍(2像素宽竖线), 球(1像素), 中线(虚线)
  //   根据cfg->show_clock: 左半场3×5 "HH", 右半场3×5 "MM"
  //   秒用底部横条: bar_len = seconds*48/60, 
  //   横条最前像素(随时间移动的头部): brightness从255→128→64→0淡入
  //   根据cfg->show_temp_humid: 左半3×5 "XX C", 右半3×5 "XX %"
  //   如本地有缓存+未过期→直接渲染
  //   如缓存过期或不存在→主动请求后端: GET /api/weather?region=X
  //   后端查和风API→返回weather JSON→设备更新渲染
  // 天气双向查询: 设备可主动请求 → GET /api/weather/{device_mac}
  //   后端返回当前天气缓存(如缓存过期则刷新)→ 设备更新天气卡片
  //   同时保留服务端推送(变化时MQTT push)

  typedef struct {
      bool show_clock;
      bool show_temp_humid;
      bool show_date;
  } pong_fg_config_t;
  ```

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (after 10) | **Blocks**: Task 25
  **Commit**: `feat(effect): Pong clock with custom foreground layout`

  **QA**: Pong球弹跳正常；HH:MM秒条正确；温湿度左右半场正确
  **Evidence**: `.sisyphus/evidence/task-16-pong.png`

- [x] 17. **前景渲染系统 + 天气背景**

  **What to do**:
  ```c
  // === 5种前景渲染函数 ===
  // components/display/effects/fg_render.c

  void fg_clock_hms(fb, x, y, font, brightness, time_t now);
  //   格式 "HH:MM:SS" — 用指定font渲染

  void fg_clock_hm(fb, x, y, font, brightness, time_t now);
  //   格式 "HH:MM"

  void fg_date_full(fb, x, y, font, brightness, time_t now);
  //   第一行 "YYYY MM DD"
  //   第二行 "SUNDAY" (全称, 居中)

  void fg_date_compact(fb, x, y, font, brightness, time_t now);
  //   一行 "MM DD SUN" (3字缩写)

  void fg_temp_humid(fb, x, y, font, brightness, float temp, float humid);
//   第一行 "Hum：XX%"
//   第二行 "Temp： XX.X C"
//   (注意: 中文冒号：, Hum缩写, Temp全写, C大写, Temp冒号后有空格)

  // === 天气背景 ===
  // components/display/effects/bg_weather.c

  typedef enum {
      WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_RAIN,
      WEATHER_SNOW, WEATHER_OVERCAST
  } weather_condition_t;

  void bg_weather_render(fb, weather_condition_t wc, weather_fg_config_t *cfg);
  //   根据wc画对应图案:
  //   - 晴: 中心太阳(圆形+放射线)
  //   - 多云: 太阳+云遮挡
  //   - 雨: 云+下落雨滴线
  //   - 雪: 云+雪花点
  //   - 阴: 大云覆盖
  //   - 大风: 风线(斜线) (从wc派生)
  //   专用前景(布尔开关, 类似Pong):
  //   - show_clock: 右半场居中3×5 "HH:MM"
  //   - show_temp_humid: 3×5 "XX C"右对齐(pad 1px), "XX %"右对齐
  //   - show_date: 3×5 "MM DD SUN" 右对齐(pad 1px)
  ```
  **参数化**: 每个前景可独立设置font_size、brightness、持续时长(用于卡片轮换)。
  **相对亮度**: `fg_brightness = bg_avg_brightness * (1 + relative_brightness/128)`

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 3 (after 10) | **Blocks**: Task 25
  **Commit**: `feat(effect): foreground renderers + weather background system`

  **QA**: 5种前景 × 3种字体全组合→拍照; 天气5种图标→拍照
  **Evidence**: `.sisyphus/evidence/task-17-fg-all.png, task-17-weather-icons.png`

---- [x] 18. **WiFi管理器（NVS存储 + STA/AP自动切换）**

  **What to do**:
  ```c
  // components/wifi_mgr/wifi_mgr.h

  typedef enum {
      WIFI_STATE_UNINIT,
      WIFI_STATE_CONNECTING,
      WIFI_STATE_CONNECTED,
      WIFI_STATE_FAILED,
      WIFI_STATE_AP_MODE
  } wifi_state_t;

  // NVS keys:
  #define NVS_KEY_SSID     "wifi_ssid"
  #define NVS_KEY_PASSWORD "wifi_pwd"

  void wifi_mgr_init(void);
  //   1. nvs_open("wifi_config")
  //   2. 读ssid/password → 如有→启动STA连接
  //   3. 如无→直接进入AP模式
  //   4. 注册事件回调:
  //      WIFI_EVENT_STA_START → esp_wifi_connect()
  //      WIFI_EVENT_STA_CONNECTED → WIFI_STATE_CONNECTED
  //      WIFI_EVENT_STA_DISCONNECTED → 重试(最多3次)
  //      IP_EVENT_STA_GOT_IP → 发布IP到Queue→屏幕显示

  void wifi_mgr_start_ap(void);
  //   SSID = "Clock-Config-XXXX" (XXXX=MAC后4位)
  //   开放网络(无密码)
  //   启动后进入WIFI_STATE_AP_MODE

  // 重试逻辑:
  //   第1次: 立即重试
  //   第2次: delay 3s → retry
  //   第3次: delay 5s → retry
  //   全部失败 → wifi_mgr_start_ap()

  // 保存凭证(瘦模式/前端调用):
  esp_err_t wifi_save_creds(const char *ssid, const char *pwd);
  //   nvs_set_str → 提交 → esp_restart()
  ```

  **Must NOT do**: 事件回调中耗时操作; STA连接无限重试

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 4 (with 19, 20, 21)
  **Commit**: `feat(wifi): NVS-backed WiFi manager with STA/AP auto-fallback`

  **QA Scenarios**:
  ```
  Scenario: 无凭证→AP模式→输入凭证→连接成功
    Tool: idf.py flash monitor
    Steps:
      1. 擦除NVS启动 → 日志 "WIFI: no creds, starting AP mode"
      2. 扫描WiFi可见 "Clock-Config-XXXX"
      3. 通过瘦模式HTTP输入有效SSID/PWD → 自动重启
      4. 重启后日志 "WIFI: connected, IP=192.168.1.x"
    Expected: 状态转换正确
    Evidence: .sisyphus/evidence/task-18-wifi-flow.txt
  ```

- [x] 19. **HTTP客户端（GET/POST + 备份API）**

  **What to do**:
  ```c
  // components/http_client/http_client.h

  typedef struct {
      char *data;    // 响应体(malloc in PSRAM)
      int   len;
      int   status_code;
  } http_response_t;

  esp_err_t http_get(const char *url, http_response_t *resp, int timeout_ms);
  //   1. esp_http_client_init: url, timeout, cert_bundle
  //   2. esp_http_client_perform
  //   3. 读取响应体 → resp->data (malloc)
  //   4. resp->status_code = 200/404/etc
  //   返回 ESP_OK / ESP_FAIL

  // 备份API调用:
  esp_err_t http_get_with_backup(
      const char *primary_url,
      const char *backup_url,
      http_response_t *resp,
      int timeout_ms);
  //   1. http_get(primary_url) → 成功返回
  //   2. 失败 → http_get(backup_url)
  //   3. 仍失败 → 返回错误

  // JSON解析辅助:
  cJSON *http_get_json(const char *url, int timeout_ms);
  //   http_get → cJSON_Parse(resp->data) → 返回cJSON对象

  void http_response_free(http_response_t *resp);
  //   free(resp->data)
  ```

  **Must NOT do**: 在主线程/display_task中调用(使用独立network_task)

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 4 (with 18, 20, 21)
  **Commit**: `feat(http): async HTTP client with backup API fallback`

  **Acceptance**: 请求https://api.ipify.org?format=json → 返回有效JSON

- [x] 20. **NTP客户端（双服务器备份）**

  **What to do**:
  ```c
  // components/ntp/ntp_client.h

  #define NTP_SERVER_PRIMARY   "ntp.aliyun.com"
  #define NTP_SERVER_BACKUP    "pool.ntp.org"
  #define NTP_SYNC_TIMEOUT_MS  10000
  #define NTP_RESYNC_INTERVAL  (6 * 3600) // 每6小时

  esp_err_t ntp_sync(void);
  //   1. esp_netif_sntp_start()
  //   2. 尝试主服务器10s超时
  //   3. 失败→尝试备服务器10s
  //   4. 成功→settimeofday()设置系统时间
  //   5. 启动定时器每6小时重同步

  // 仅在时区已设置后调用此函数
  // 前置条件: Task 22完成, setenv("TZ",...)已执行
  ```

  **Must NOT do**: 时区未设置前启动NTP

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 4 (with 18, 19, 21)
  **Commit**: `feat(ntp): SNTP client with ntp.aliyun.com/pool.ntp.org fallback`

  **QA**: NTP同步后 `date` 命令输出与手机时间对比误差<1s

- [x] 21. **瘦模式HTTP服务器**

  **What to do**:
  ```c
  // components/thin_mode/thin_http.h

  // 仅WiFi AP模式时启动
  void thin_http_start(void);
  //   1. httpd_start() on port 80
  //   2. 注册路由:

  // GET / → serve_config_page()
  //   返回SPIFFS中存储的HTML页面(thin_config.html)
  //   HTML内容:
  //   <form action="/api/wifi-config" method="POST">
  //     <label>WiFi SSID (仅2.4GHz):</label>
  //     <select name="ssid"> <!-- JS扫描附近WiFi填充 -->
  //     <label>Password:</label>
  //     <input type="password" name="pwd">
  //     <label>后端地址:</label>
  //     <input name="backend_url">
  //     <label>备用后端:</label>
  //     <input name="backup_url">
  //     <button>保存并重启</button>
  //   </form>

  // POST /api/wifi-config
  //   解析表单 → wifi_save_creds(ssid, pwd)
  //   如有backend_url → nvs_set_str("backend_url", url)
  //   返回200 → 设备重启

  // HTML存储到SPIFFS:
  //   spiffs分区挂载 → open("/spiffs/static/thin_config.html")
  ```

  **Must NOT do**: 胖模式运行HTTP server(省内存); HTML>50KB

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 4 (with 18, 19, 20)
  **Commit**: `feat(thin): AP mode captive portal for WiFi/backend configuration`

  **QA**: Playwright: 连接Clock热点 → 打开192.168.4.1 → 填表提交 → 设备重启连接 → 验证
  **Evidence**: `.sisyphus/evidence/task-21-thin-config.png`

- [x] 22. **IP定位 + 时区查询流水线**

  **What to do**:
  ```c
  // components/geo/geo_pipeline.h

  typedef struct {
      char public_ip[46];     // IPv4: max 15 chars
      char country[64];
      char region[64];        // 省/州
      char city[64];          // 市
      char district[64];      // 区 (中国)
      char timezone[64];      // IANA时区如"Asia/Shanghai"
      float lat, lng;
  } geo_info_t;

  esp_err_t geo_pipeline_execute(geo_info_t *info);
  //   Step 1 — 获取公网IP:
  //     主: https://api.ip.sb/geoip → resp.ip
  //     备: https://api.ipify.org?format=json → resp.ip
  //
  //   Step 2 — IP→地区:
  //     主: https://ipvx.netart.cn/?ip={ip} → regions[0,1,2] → 省,市,区
  //     备: http://ip-api.com/json/{ip}?fields=country,regionName,city,district
  //
  //   Step 3 — 地区→时区:
  //     主: http://api.timezonedb.com/v2.1/get-time-zone?
  //           key=TZDB_API_KEY&by=position&lat={lat}&lng={lng}
  //     备: http://worldtimeapi.org/api/timezone/{Area}/{City}
  //
  //   每一步: 主API失败→自动切备; 每步通过Queue向display_task发状态更新
  //   结果缓存NVS: 下次启动先读缓存, 如小于24小时可直接用

  // NVS缓存:
  //   nvs_set_str("geo_cache", json_serialized) + timestamp
  ```

  **Must NOT do**: display_task中等待API响应; 缓存过期不用

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 5 (after 18, 19)
  **Commit**: `feat(geo): 3-step IP→region→timezone pipeline with NVS cache`

  **QA**: 上电后日志显示三步骤成功，地区精确到区级
  **Evidence**: `.sisyphus/evidence/task-22-geo-pipeline.txt`

- [x] 23. **NTP自动对时编排**

  **What to do**:
  ```c
  // components/geo/auto_sync.c

  void auto_sync_task(void *pv);
  //   1. 等待WiFi连接 (EventGroup bit WIFI_CONNECTED)
  //   2. geo_pipeline_execute() → 获取时区
  //   3. setenv("TZ", info.timezone, 1); tzset();
  //   4. ntp_sync() → 设置系统时间
  //   5. 通过Queue通知display_task: "ALL_READY"
  //   6. 删除此任务(一次性)
  //
  //   全流程<30秒, 每步状态在屏幕显示:
  //   显示"WiFi ✓" → "IP ✓" → "Region ✓" → "TZ ✓" → "NTP ✓"
  ```

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 5 (after 20, 22)
  **Commit**: `feat(sync): orchestrate WiFi→IP→timezone→NTP auto pipeline`

  **QA**: 上电30s内完整流程完成, `date` 输出时间+时区正确
  **Evidence**: `.sisyphus/evidence/task-23-auto-sync.txt`

---- [x] 24. **MQTT客户端（胖瘦模式切换）**

  **What to do**:
  ```c
  // components/mqtt_client/mqtt_client.h

  // 主题约定:
  //   发布: clock/{mac}/status     → {"online":bool, "version":"1.0.0", "hw":"0.1", "ip":"x", "region":"x"}
  //   订阅: clock/{mac}/config     → 卡片配置JSON
  //   订阅: clock/{mac}/ota        → {"url":"...", "version":"x"}
  //   订阅: clock/{mac}/weather    → {"condition":"晴", "wind":"3级"}

  void mqtt_client_init(const char *broker_url);
  //   1. esp_mqtt_client_init: client_id=MAC, LWT=topic=status, payload="offline"
  //   2. 注册事件回调: MQTT_EVENT_CONNECTED → 发布online状态+订阅topic
  //      MQTT_EVENT_DISCONNECTED → 启动重连计时器
  //      MQTT_EVENT_DATA → 解析topic, 投递到对应Queue

  // 重连策略:
  //   断开→1s→重试; 再断开→2s→重试; 4s→8s→...→max 60s
  //   连续3次连接失败 → 标记后端不可达 → 切瘦模式
  //   瘦模式中每2分钟尝试一次重连 → 成功→退出瘦模式

  void mqtt_publish_status(void);
  //   发布 clock/{mac}/status: online, version, hw_rev, ip, region, timezone
  ```
  **胖瘦模式切换**: mqtt_task检测连接状态 → 失败3次→`thin_http_start()` → 每2分钟重试MQTT

  **Must NOT do**: MQTT回调中阻塞; display_task中处理MQTT消息

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 6 (after 18)
  **Commit**: `feat(mqtt): MQTT client with LWT, exponential backoff, fat/thin mode`

  **QA Scenarios**:
  ```
  Scenario: MQTT断开→瘦模式→恢复
    Steps:
      1. Mosquitto运行→设备连接→日志"MQTT: connected"
      2. 停Mosquitto→3次重试→日志"MQTT: backend unreachable, thin mode"
      3. 启Mosquitto→2分钟后日志"MQTT: reconnected"
    Expected: 自动切换+恢复
    Evidence: .sisyphus/evidence/task-24-mqtt-failover.txt
  ```

- [x] 25. **卡片管理器（9卡+约束验证+按键切换）**

  **What to do**:
  ```c
  // components/cards/card_mgr.h

  #define MAX_CARDS  9
  #define MAX_FG_PER_CARD  5

  typedef struct {
      fg_type_t type;
      font_size_t font;      // 3x5/5x5/7x9
      uint16_t duration_ms;  // 该前景显示时长(用于切换)
  } fg_slot_t;

  typedef struct {
      bg_type_t bg;
      fg_slot_t fgs[MAX_FG_PER_CARD];
      uint8_t fg_count;
      int8_t relative_brightness; // -128到+127: 负=前景比背景暗
      uint16_t switch_interval;   // 秒, 0=不自动切换
  } card_t;

  static card_t cards[MAX_CARDS];
  static uint8_t card_count;     // 实际卡片数(1-9)
  static uint8_t current_card;   // 当前显示索引
  static uint8_t gcc_value;      // 当前全局亮度(0-127)

  // 初始化(启动时从NVS加载):
  void card_mgr_init(void);

  // 切换:
  void card_next(void);  // current_card = (current+1) % card_count
  void card_set(uint8_t idx);

  // 约束验证:
  bool card_validate(const card_t *card);
  //   if (card->bg == BG_FIRE && card->fg_count > 0)
  //     检查所有fg的font == FONT_3x5, 否则返回false
  //   if (card->bg == BG_WATER_RIPPLE || BG_HOURGLASS || BG_GAME_OF_LIFE)
  //     return (card->fg_count == 0);  // 禁止前景
  //   if (card->bg == BG_CODE_RAIN && card->fg_count > 0)
  //     前景区域自动作为mask → 需要计算mask_rect
  //   if (card->bg == BG_PONG || BG_WEATHER)
  //     fg不使用通用font设置, 而是fg_slot.type决定显示哪些项(布尔开关)

  // 亮度:
  void brightness_adjust(bool increase);
  //   if increase: gcc = min(127, gcc + 5)
  //   else:        gcc = max(5, gcc - 5)
  //   到头(0或127)停止; 松手再按反向

  // 持久化:
  void card_mgr_save_to_nvs(void);  // 序列化为JSON→NVS
  void card_mgr_load_from_nvs(void);
  ```

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 6 (after 9, 17)
  **Commit**: `feat(cards): 9-card manager with constraint validation and button control`

  **QA**: 双击切换环形; 定时自动切换; 火焰+非3×5前景被拒绝

- [x] 26. **配置同步协议（NVS ↔ MQTT）**

  **What to do**:
  ```c
  // 配置JSON格式 (NVS存储 + MQTT传输):
  // {
  //   "v": 1,                    // 配置版本号
  //   "wifi": {"ssid":"x","pwd":"x"},
  //   "backend": {"primary":"...","backup":"..."},
  //   "cards": [
  //     {"bg":0, "fgs":[{"t":0,"f":1,"d":5000}], "rb":0, "sw":10},
  //     ...最多9个
  //   ]
  // }

  // 同步逻辑:
  void config_sync_on_mqtt_message(const char *json);
  //   1. 解析JSON → 提取version
  //   2. 比对本地NVS中的version
  //   3. 如果远程version > 本地version → 覆盖NVS → 热应用
  //      (card_mgr_reload_from_json → 不需要重启)
  //   4. 如果version相同 → 忽略
  //   5. 如果远程version < 本地 → 上报本地版本(服务端应纠正)

  void config_report_to_server(void);
  //   MQTT连接后自动发布当前配置+版本号
  //   topic: clock/{mac}/config_report
  ```

  **Must NOT do**: 每次同步全量重写NVS(只写差异字段); 同步导致显示卡顿

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 6 (after 24, 25)
  **Commit**: `feat(config): NVS↔MQTT config sync with versioning and hot-apply`

- [x] 27. **主应用任务编排**

  **What to do**:
  ```c
  // main/app.c

  void app_main(void) {
      // 1. 初始化顺序:
      nvs_flash_init();
      // 加载配置(读NVS: wifi凭证, backend URL, cards, geo缓存)
      config_load_all();

      // 2. 硬件初始化:
      gpio_install_isr_service(0);
      button_init();          // IO38中断
      i2c_master_init();      // IO1=SCL, IO2=SDA
      tca9548a_init();
      // 遍历ch0-5初始化6个3729
      for (int ch=0; ch<6; ch++) {
          tca9548a_select(ch);
          is31fl3729_init(ADDR_FOR_CH(ch));
      }
      aht21_init();           // 主线0x38

      // 3. 显示初始化:
      framebuffer_init();
      // 创建display_task (PRIO 5)
      xTaskCreate(display_task, "display", 8192, NULL, PRIO_DISPLAY, NULL);
      // 创建render_task (PRIO 4)
      xTaskCreate(render_task, "render", 16384, NULL, PRIO_RENDER, NULL);

      // 4. 网络初始化:
      wifi_mgr_init(); // 自动尝试连接或切AP
      // 创建wifi_task, mqtt_task, network_task

      // 5. 按键→动作映射:
      //   在button_task中:
      //     case BTN_SINGLE_CLICK: toggle_display();
      //     case BTN_DOUBLE_CLICK: card_next();
      //     case BTN_LONG_PRESS_HOLD: brightness_adjust(direction);
      //     case BTN_LONG_PRESS_RELEASE: flip direction;

      // 6. 主循环:
      while(1) {
          // 处理系统事件, 监控WiFi/MQTT状态, 触发auto_sync等
          vTaskDelay(1000);
      }
  }
  ```

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 6 (after 24-26)
  **Commit**: `feat(app): main application orchestration with error recovery`

- [x] 28. **OTA处理器（双分区+回滚+中断恢复）**

  **What to do**:
  ```c
  // components/ota/ota_handler.h

  #define OTA_URL_MAX_LEN 256

  void ota_start(const char *fw_url, const char *expected_sha256);
  //   1. 停止非关键任务: 通知音频(Phase2钩子), 暂停render_task
  //      display_task继续运行(显示OTA进度)
  //   2. esp_https_ota_config_t: url, cert_bundle, timeout=120s
  //   3. esp_https_ota(&config): 下载→写入空闲OTA分区
  //      进度回调→更新屏幕进度条
  //   4. 验证SHA256(如提供) → 不匹配→擦除+报错
  //   5. esp_ota_set_boot_partition(updated_partition)
  //   6. esp_restart()

  // 回滚保护(启动时调用):
  void ota_check_rollback(void);
  //   esp_ota_get_state_partition:
  //   - 如果启动成功: esp_ota_mark_app_valid_cancel_rollback()
  //   - 如果崩溃: bootloader自动回滚到上一有效分区
  //     → 下次启动运行旧固件 → 上报MQTT: ota_rolled_back

  // 中断恢复:
  //   如果下载中途断电:
  //   - 重启后esp_ota_get_state_partition → PENDING_VERIFY
  //   - 检测到中断→擦除未完成分区→重新下载
  //   (不保留部分下载, 避免flash碎片)

  // MQTT触发:
  void ota_on_mqtt_trigger(const char *json);
  //   解析{"url":"https://...","version":"2.0.0","sha256":"abc..."}
  //   → ota_start(url, sha256)
  ```

  **Must NOT do**: OTA期间不擦除NVS; factory分区不可被覆盖

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 7 (after 19, 24)
  **Commit**: `feat(ota): HTTPS OTA with dual-partition rollback and crash recovery`

  **QA Scenarios**:
  ```
  Scenario: OTA正常升级 + 中断后恢复
    Steps:
      1. 上传v2.0固件到后端→触发OTA→设备下载→验证→切换→重启
      2. 重启后MQTT上报version=v2.0
      3. 再触发OTA, 下载中途拔电→重启→自动回滚到v2.0(或重新下载)
    Expected: 升级成功; 中断后不砖
    Evidence: .sisyphus/evidence/task-28-ota-success.txt, task-28-ota-rollback.txt
  ```

- [x] 29. **固件版本上报 + HW版本自动检测**

  **What to do**:
  ```c
  // components/version/version.h
  #define FW_VERSION "1.0.0"   // 编译时通过 -DPROJECT_VER="1.0.0" 注入
  #define HW_REV      autodetect_hw_rev()

  // 自动检测硬件版本:
  const char* autodetect_hw_rev(void) {
      // 扫描I2C地址 0x70-0x77 (TCA9548A地址范围)
      // 如果发现 0x70 (或其他TCA9548A地址) → return "0.1"
      // 否则 → return "0.2"
  }

  // MQTT连接后上报:
  void version_report(void) {
      cJSON *root = cJSON_CreateObject();
      cJSON_AddStringToObject(root, "mac", get_mac_str());
      cJSON_AddStringToObject(root, "fw_version", FW_VERSION);
      cJSON_AddStringToObject(root, "hw_rev", HW_REV);
      cJSON_AddStringToObject(root, "ip", get_ip_str());
      cJSON_AddStringToObject(root, "region", geo_info.region);
      cJSON_AddStringToObject(root, "timezone", geo_info.timezone);
      mqtt_publish("clock/{mac}/status", cJSON_Print(root));
  }
  ```

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 7 (after 24)
  **Commit**: `feat(version): auto-detect HW revision and report firmware version`

---

- [x] 30. **FastAPI项目 + DB Schema + Docker**

  **What to do**:
  ```
  backend/
  ├── app/
  │   ├── main.py, config.py, database.py
  │   ├── models/ (user, device, card, firmware, weather_cache)
  │   ├── schemas/ (Pydantic request/response)
  │   ├── routers/ (auth, admin, user, device)
  │   ├── services/ (mqtt_bridge, weather_svc, ota_svc)
  │   └── middleware/auth.py
  ├── alembic/ + requirements.txt + Dockerfile
  ```
  **PostgreSQL表**: users(id, username, pwd_hash, is_admin) | devices(mac PK, hw_rev, fw_ver, owner_id FK, ip, region, tz, backend_urls JSONB, last_seen) | cards(id, device_mac FK, position, bg_type, fg_config JSONB, duration, rel_brightness) | firmwares(id, hw_rev, version, file_path, sha256) | weather_cache(region PK, condition, wind, updated_at)
  **Docker Compose**: fastapi + postgres:15 + mosquitto:2, 网络互联, 卷挂载固件目录。

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 8 (with 31, 32, 33)
  **Commit**: `feat(backend): FastAPI project, SQLAlchemy async, Alembic, Docker Compose`

- [x] 31. **认证系统（Session + MAC白名单）**

  **What to do**:
  ```python
  # POST /api/auth/login → bcrypt验证→session["user_id"]=user.id
  # POST /api/auth/logout → session.clear()
  # 设备: 请求头 X-Device-MAC → 查Device表→通过;
  #   设备只能操作自己的资源(URL中mac==请求头MAC)
  # 管理员: session["is_admin"]==True → 访问/admin/*路由
  ```

  **Must NOT do**: JWT; 硬编码密码
  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 8
  **Commit**: `feat(auth): session-based auth + MAC whitelist middleware`

- [x] 32. **Mosquitto配置 + 后端MQTT桥接**

  **What to do**:
  ```python
  # app/services/mqtt_bridge.py: paho-mqtt连接localhost Mosquitto
  # 订阅: clock/+/status→自动注册设备+更新last_seen
  #       clock/+/config_report→存储配置版本号
  #       clock/+/ota_status→更新OTA进度
  # 发布: clock/{mac}/config(卡片), clock/{mac}/ota(固件URL), clock/{mac}/weather(天气)
  ```
  **Mosquitto ACL**: clock/+ 设备可读写; clock/+/# 后端全权限

  **Recommended Agent Profile**: `quick` | **Skills**: `[]`
  **Parallel**: Wave 8
  **Commit**: `feat(mqtt): Mosquitto ACL + backend bridge for device communication`

- [x] 33. **和风天气服务（15min缓存+变化推送）**

  **What to do**:
  ```python
  # app/services/weather_svc.py
  # GET https://devapi.qweather.com/v7/weather/now?location={id}&key={KEY}
  # ...
  # 设备主动查询API:
  # GET /api/weather/{device_mac}
  #   后端根据device的region查weather_cache→返回weather JSON
  #   如缓存过期→主动刷新→返回最新
  #   不做MQTT推送(仅pull响应)
  ```

  **Recommended Agent Profile**: `deep` | **Skills**: `[]`
  **Parallel**: Wave 8
  **Commit**: `feat(weather): HeFeng API with 15-min cache, bidirectional query (push+pull)`

- [x] 34. **设备注册API**

  **What to do**: MQTT status消息→upsert Device(mac, hw_rev, fw_ver, ip, region, tz, last_seen=now)。`GET /api/admin/devices`(列表+过滤), `GET /api/user/devices`(用户自己的)。
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 9
  **Commit**: `feat(api): auto device registration + admin/user device listing`

- [x] 35. **用户-设备绑定**

  **What to do**: `POST /api/admin/bind`(user_id+device_mac, 校验设备未绑定他人→一机一用户)。`POST /api/admin/unbind`。未绑定设备不报错。
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 9
  **Commit**: `feat(api): device-user binding with one-device-one-user constraint`

- [x] 36. **设备详情/删除/后端URL**

  **What to do**: `GET /api/admin/devices/{mac}`(详情+卡片列表+OTA状态)。`DELETE /api/devices/{mac}`(删关联card→解绑→删device)。`PUT /api/admin/backend-urls`(设主/备URL→MQTT推送)。
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 9
  **Commit**: `feat(api): device detail, deletion, backend URL management`

- [x] 37. **服务版本 + 统计API**

  **What to do**: `GET /api/version` → `{server_ver, supported_hw_revs, compatible_fw_versions}`。`GET /api/admin/stats` → 设备/在线/用户/卡片统计数。
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 9
  **Commit**: `feat(api): server version info + dashboard statistics`

- [x] 38. **卡片CRUD API (含完整约束校验)**

  **What to do**:
  ```python
  # POST /api/user/devices/{mac}/cards → 校验用户是owner
  # validate_card_constraints(card):
  #   火焰→所有fg.font必须FONT_3x5, 否则400
  #   水纹/沙漏/生命→fg_count必须0, 否则400
  #   代码雨→自动计算mask_rect(根据前景类型/位置/字体)
  #   Pong/天气→不设font, fg.type是布尔开关(show_clock等)
  #   卡片数≤9 → 否则400
  #   保存→自动推送设备
  ```

  **Recommended Agent Profile**: `deep` | **Parallel**: Wave 10
  **Commit**: `feat(api): card CRUD with per-background constraint validation`

- [x] 39. **卡片保存后自动推送**

  **What to do**: CRUD后→MQTT发布`clock/{mac}/config`→含递增version号→设备热应用
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 10
  **Commit**: `feat(push): auto-push card config to device on save`

- [x] 40. **固件上传 + 版本管理**

  **What to do**: `POST /api/admin/firmwares`(multipart .bin+hw_rev+version→保存+算SHA256)。`GET /api/admin/firmwares`(列表,按hw_rev分组)。用户登录时比对设备版本→前端提醒升级。
  **Recommended Agent Profile**: `quick` | **Parallel**: Wave 10
  **Commit**: `feat(api): firmware upload with hw_rev targeting + upgrade reminder`

- [x] 41. **OTA触发 + 状态追踪**

  **What to do**: `POST /api/admin/devices/{mac}/ota`→MQTT推送固件URL+version+SHA256。`GET /api/admin/devices/{mac}/ota-status`→(idle/downloading/installing/success/rolled_back)。MQTT ota_status上报更新进度。
  **Recommended Agent Profile**: `deep` | **Parallel**: Wave 10
  **Commit**: `feat(api): OTA trigger with real-time status tracking`

- [x] 42. **Vue3前端项目初始化**

  **What to do**: `frontend/` — Vite+Vue3+TypeScript+Element Plus+Axios+Vue Router。路由: /login, /admin/*(管理员), /user/*(用户)。通用Layout(Element Plus Container+Menu)。API client封装(axios带session cookie+baseURL)。
  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 11 (after 30)
  **Commit**: `feat(frontend): Vue3+Element Plus project with routing and API client`

- [x] 43. **管理员面板**

  **What to do**:
  - Dashboard: 设备数/在线/用户/卡片统计
  - 设备管理: Table(MAC/HW/版本/IP/地区/绑定用户/在线/操作)。操作: 删除/查看卡片/OTA触发。OTA弹窗选固件版本→确认触发→实时进度条。
  - 固件管理: 上传(选文件+hw_rev+版本号)→列表(按hw_rev分组)
  - 用户管理: 列表→绑定/解绑设备(Dialog选用户+选设备)
  - 后端URL配置: 表单设主/备URL→保存

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 11 (after 42)
  **Commit**: `feat(frontend): admin panel - devices, firmware, users, bindings`

- [x] 44. **用户面板 — 卡片编辑器（核心）**

  **What to do**:
  - 设备选择(卡片列表显示我的设备)
  - 卡片编辑器: 9个槽位(垂直列表/拖拽排序)
  - 属性面板:
    - 背景: 7选1 dropdown → 选后显示约束提示
    - 前景: 动态表单(根据背景类型变化):
      - **通用背景**(火焰/代码雨): 前景多选(时钟HMS/时钟HM/日期全/日期简/温湿度) → 每前景独立: 字体选择(3x5/5x5/7x9/更长字体)+持续时长(秒)
      - **禁前景***(水纹/沙漏/生命): 前景区域disabled+提示
      - **Pong/天气**: 前景布尔开关(是否显示时钟/温湿度/日期)
    - 相对亮度: slider(-128~+127)
    - 自动切换秒数: input number(0=不自动)
  - 实时约束校验(前端+后端双重)
  - 保存→自动推送到设备

  **Recommended Agent Profile**: `visual-engineering` | **Skills**: `[]`
  **Parallel**: Wave 11 (after 42)
  **Commit**: `feat(frontend): card editor with dynamic constraint validation and preview`

- [x] 45. **端到端集成测试**

  **What to do**: pytest: 完整链路→固件上电→WiFi→IP→NTP→MQTT注册→后端创建设备→管理员绑定→创建卡片→MQTT推送→验证设备MQTT ACK→OTA触发→验证升级。
  **Recommended Agent Profile**: `deep` | **Parallel**: Wave 12
  **Commit**: `test(e2e): full device life cycle integration test`

- [x] 46. **错误场景测试**

  **What to do**: 测试: ①WiFi断连→AP回退 ②MQTT断开→瘦模式→恢复 ③IP API超时→备API ④NTP主失败→备 ⑤OTA中断→回滚 ⑥无效卡片→400 ⑦未绑定设备→不报错 ⑧双设备同MAC→冲突处理
  **Recommended Agent Profile**: `deep` | **Parallel**: Wave 12
  **Commit**: `test(error): robustness coverage for all failure modes`

- [x] 47. **24h稳定性测试**

  **What to do**: 设备运行24h+监控: 帧率(≥30fps), heap_free(无泄漏), WiFi断连次数, MQTT断连次数, NVS写入量, CPU占用趋势。
  **Recommended Agent Profile**: `deep` | **Parallel**: Wave 12
  **Commit**: `test(stability): 24h+ endurance with resource monitoring`

- [x] 48. **文档 + 部署指南**

  **What to do**: README(项目概述/结构/硬件接线/快速开始)。DEPLOY.md(Docker Compose一键部署)。API文档(自动Swagger)。固件编译烧录步骤。
  **Recommended Agent Profile**: `writing` | **Parallel**: Wave 12
  **Commit**: `docs: README, hardware wiring, deployment, API reference`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Get explicit user "okay".

- [x] F1. **Plan Compliance Audit** — `oracle`
  Verify each "Must Have" exists. Verify each "Must NOT Have" absent. Check `.sisyphus/evidence/` completeness. Output: `Must Have [N/N] | Must NOT Have [N/N] | VERDICT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `idf.py build` + `pytest` + `npm run build`. Check for: `@ts-ignore`, empty catches, console.log, unused imports, hardcoded keys. Output: `Build | Lint | Tests | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high` (+ `playwright`)
  Execute ALL QA scenarios from every task. Start clean. Test cross-task integration. Capture evidence to `.sisyphus/evidence/final-qa/`. Output: `Scenarios [N/N] | Integration [N/N] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  Each task: verify implementation matches spec. Check "Must NOT do" compliance. Flag unaccounted changes. Output: `Tasks [N/N] | Contamination [CLEAN/N] | VERDICT`

---

## Commit Strategy

常规: `type(scope): desc` · TDD先提交测试再实现 · 每组完成后运行测试

---

## Success Criteria
```bash
cd firmware && idf.py build        # 无错误
cd backend && pytest               # ALL PASS
cd frontend && npm run build       # 无错误
curl :8000/api/health              # {"status":"ok"}
```
### Final Checklist
- [x] All Must Have present · All Must NOT Have absent · All tests pass · Display 30fps+
