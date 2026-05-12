# 桌面时钟 ESP32-S3 烧录与硬件验证指南

固件版本: 0.1.0 | 硬件版本: 0.1 | 目标芯片: ESP32-S3 (16MB Flash)

---

## 1. 前置条件

### 1.1 ESP-IDF v5.5 安装

验证你的 ESP-IDF 安装是否存在且版本匹配：

```bash
ls ~/.espressif/v5.5.4/esp-idf/
```

若目录存在，执行导出脚本并确认：

```bash
. ~/.espressif/v5.5.4/esp-idf/export.sh
idf.py --version
```

本指南全文使用安装路径 `~/.espressif/v5.5.4/esp-idf/`。若你的 ESP-IDF 安装在其他位置，请相应调整。

### 1.2 USB 驱动

| 操作系统 | 所需驱动 |
|---------|-----------------------------------------------------|
| Linux | 无（CDC-ACM 内核模块，自动加载） |
| macOS | 无（内建 CDC-ACM 支持） |
| Windows | [CP210x USB to UART Bridge VCP Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |

ESP32-S3 板载 USB 转 UART 桥接器使用 CP2102 或 CH343 芯片。在 Linux 上，验证驱动是否已加载：

```bash
lsmod | grep cp210x
# 或
lsmod | grep ch341
```

若没有任何输出，说明驱动已编译进内核（现代发行版的常见情况）。

### 1.3 硬件连接

1. 使用 USB **数据线**（Type-C 转 Type-A 或 Type-C 转 Type-C）。纯充电线无法工作。
2. 将 Type-C 端连接到 ESP32-S3 开发板。
3. 将 USB-A（或 Type-C）端连接到你的电脑。

### 1.4 验证串口

连接后，开发板会显示为一个串口设备。

**Linux：**

```bash
ls /dev/ttyUSB*
# 预期: /dev/ttyUSB0（若存在其他设备可能为 /dev/ttyUSB1）

# 通过内核消息的替代检查方式:
dmesg | grep -i "cp210x\|ch341\|ttyUSB" | tail -5
```

**macOS：**

```bash
ls /dev/cu.*
# 预期: /dev/cu.usbserial-XXXX 或 /dev/cu.SLAB_USBtoUART
```

**Windows：**

在设备管理器中查看"端口 (COM 和 LPT)"下的"Silicon Labs CP210x USB to UART Bridge (COMx)"。记录 COM 端口号（如 COM3）。

如果没有设备出现：
- 验证 USB 数据线确实是数据线（用其他设备测试）。
- 尝试电脑上另一个 USB 端口。
- 在 Windows 上，安装第 1.2 节中链接的 CP210x 驱动。

---

## 2. 首次烧录（出厂固件）

### 2.1 设置目标并构建

```bash
# 进入固件目录
cd ~/260510_clock/firmware

# 加载 ESP-IDF 环境
. ~/.espressif/v5.5.4/esp-idf/export.sh

# 设置目标芯片（一次性操作，创建 sdkconfig）
idf.py set-target esp32s3

# 构建固件
idf.py build
```

说明：
- `set-target esp32s3` 将 `CONFIG_IDF_TARGET_ESP32S3=y` 写入 sdkconfig。`sdkconfig.defaults` 文件已经指定了此项及其他设置（PSRAM、16MB flash、自定义分区表、OTA、WiFi、MQTT、证书包、SPIFFS），因此大部分配置已预先应用。
- `idf.py build` 编译所有组件：主固件（app_main.c、render_task.c）加上 17 个组件库（I2C 多路复用器、LED 矩阵驱动、显示、WiFi 管理器、NTP、MQTT、卡片、瘦模式、OTA 等）。

### 2.2 烧录并监控

```bash
# 将 /dev/ttyUSB0 替换为你的实际端口
idf.py -p /dev/ttyUSB0 flash monitor
```

这条命令完成以下操作：
1. 擦除并向 flash 写入 bootloader、分区表和出厂固件。
2. 以 115200 波特率（ESP-IDF 默认值）打开串口监控。

**波特率：** 115200（由 ESP-IDF 监控器设置，无法通过此命令配置）。

**退出监控：** 按下 `Ctrl+]`（Control + 右方括号）。在某些键盘上为 `Ctrl` + `]`（`P` 键右侧的键）。

如果烧录步骤因连接错误而失败：

1. 按住 ESP32-S3 开发板上的 **BOOT** 按钮。
2. 按住 BOOT 的同时，按下并释放 **RST**（EN）按钮。
3. 释放 BOOT。
4. 芯片进入下载模式。重新执行烧录命令。

或者在某些开发板上，在整个烧录过程中按住 BOOT（直到看到"Writing at 0x..."）也能达到相同效果。

### 2.3 首次启动流程

首次启动时，固件按以下步骤执行（可在串口监控中查看）：

1. NVS flash 初始化（损坏时自动擦除）。
2. 从 eFuse 读取 MAC 地址。
3. I2C 总线初始化（GPIO1=SCL，GPIO2=SDA，400kHz）。
4. SDB 引脚（GPIO8）拉低，然后通过 TCA9548A 多路复用器（6 通道）初始化 LED 驱动。
5. 分配显示帧缓冲，创建显示任务（优先级 5）。
6. 创建渲染任务（优先级 4）。
7. 卡片管理器从 NVS 加载已保存的配置。
8. WiFi 管理器启动。由于没有已保存的凭据，进入 **AP 模式**。
9. 开发板创建名为 `Clock-Config-XXXX`（其中 XXXX 从 MAC 地址派生）的 WiFi 接入点。
10. 主循环等待按键事件。

如果之前已保存 WiFi 凭据且存储的网络可达，固件将改为：
- 以 STA 模式连接 WiFi。
- 通过 IP 执行地理定位。
- 通过 NTP 同步时间。
- 连接到已配置后端 URL 的 MQTT Broker。
- 开始显示时钟画面。

---

## 3. I2C 设备扫描（硬件验证）

I2C 扫描固件测试主 I2C 总线及 TCA9548A 多路复用器全部 6 个通道的信号完整性。

### 3.1 修改 CMakeLists.txt

编辑 `firmware/main/CMakeLists.txt`。将 `SRCS` 行改为仅包含 `i2c_scan.c`：

```cmake
idf_component_register(
    SRCS "i2c_scan.c"
    INCLUDE_DIRS "."
    REQUIRES config log shared display led_matrix i2c_mux sensors button
              wifi_mgr http_client ntp geo mqtt_client cards thin_mode ota version
)
```

原始行内容为：
```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

`REQUIRES` 行可以保持不变。i2c_scan.c 固件源码不使用这些组件库，但保留它们不会造成影响（UNUSED 符号不会被链接）。

### 3.2 构建并烧录

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3.3 预期输出

扫描依次执行 4 项测试：

```
========================================
  I2C Bus Verification - Desktop Clock
========================================

--- Test 1: SDB=LOW (all LED drivers disabled) ---

=== Main Bus (SDB=LOW) ===
  Found device at 0x38 [AHT21]
  Found device at 0x70 [TCA9548A]
  Total: 2 device(s)

--- Test 2: SDB=HIGH (all LED drivers enabled) ---

=== Main Bus (SDB=HIGH) ===
  Found device at 0x38 [AHT21]
  Found device at 0x70 [TCA9548A]
  Total: 2 device(s)

--- Test 3: Through TCA9548A channels (ch0-ch5) ---

=== TCA9548A Channel 0 ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

=== TCA9548A Channel 1 ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

... (channels 2 through 5, each showing 1 device at 0x34)

--- Test 4: 400kHz I2C scan (first 2 channels) ---

=== Channel 0 @ 400kHz ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

=== Channel 1 @ 400kHz ===
  Found device at 0x34 [IS31FL3729]
  Total: 1 device(s)

========================================
  Verification Complete
========================================
```

**预期设备汇总：**

| 设备 | I2C 地址 | 所在位置 |
|--------------|-------------|----------------------|
| AHT21 | 0x38 | 主总线（直连） |
| TCA9548A | 0x70 | 主总线（直连） |
| IS31FL3729 | 0x34 | 复用器通道 0 |
| IS31FL3729 | 0x34 | 复用器通道 1 |
| IS31FL3729 | 0x34 | 复用器通道 2 |
| IS31FL3729 | 0x34 | 复用器通道 3 |
| IS31FL3729 | 0x34 | 复用器通道 4 |
| IS31FL3729 | 0x34 | 复用器通道 5 |

总计：**8 个设备**。

### 3.4 I2C 扫描故障排除

**仅发现 2 个设备（0x38 和 0x70，无通道设备）：**
- 检查 SDB（GPIO8）是否被拉高。扫描在通道测试前将 SDB 设为 HIGH。用万用表验证 GPIO8 测量值约 3.3V。
- 检查 GPIO8 与全部 6 个 IS31FL3729 芯片 SDB 引脚之间的物理连接。

**未发现任何设备：**
- 验证 I2C 上拉电阻存在于 SDA（GPIO2）和 SCL（GPIO1）上。每根信号线应有 2.2k 至 4.7k 的上拉到 3.3V。
- 检查 I2C 总线上是否有短路或虚焊。
- 验证 ESP32-S3 供电正常（3.3V 电压轨稳定）。

**通道设备少于 6 个：**
- 检查 TCA9548A 多路复用器接线。每个通道（SD0-SD5，引脚 4-9）应连接到一片 IS31FL3729 的 SDA。每个通道（SC0-SC5，引脚 14-19）应连接到一片 IS31FL3729 的 SCL。
- 验证每片 IS31FL3729 的地址引脚：AD0 和 AD1 必须都接 GND 以获得地址 0x34。

**400kHz 扫描失败但 100kHz 通过：**
- 表明信号完整性处于临界状态。检查上拉电阻值。400kHz 下建议使用 2.2k 上拉。
- 检查总线电容是否过大（长导线、多设备）。

### 3.5 恢复 CMakeLists.txt

验证完成后，恢复原始 SRCS 行：

```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

---

## 4. GPIO 测试（引脚验证）

GPIO 测试固件将每个输出引脚依次置为 HIGH 再 LOW，允许用万用表进行电压验证。

### 4.1 修改 CMakeLists.txt

将 SRCS 改为仅包含 `gpio_test.c`：

```cmake
idf_component_register(
    SRCS "gpio_test.c"
    ...
)
```

### 4.2 构建并烧录

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 4.3 测试步骤

固件在无限循环中依次遍历每个引脚。每个引脚先置 HIGH 1 秒，再置 LOW 1 秒。

**串口输出：**

```
I (xxx) gpio_test: === Test Cycle 1 ===
I (xxx) gpio_test: Button state: released
I (xxx) gpio_test: I2C_SCL (GPIO1): skipping (I2C peripheral)
I (xxx) gpio_test: I2C_SDA (GPIO2): skipping (I2C peripheral)
I (xxx) gpio_test: >>> SETTING SDB (IS31FL3729 Shutdown) (GPIO8) HIGH <<<
I (xxx) gpio_test: >>> SETTING SDB (IS31FL3729 Shutdown) (GPIO8) LOW <<<
I (xxx) gpio_test: >>> SETTING LED (Status LED) (GPIO4) HIGH <<<
I (xxx) gpio_test: >>> SETTING LED (Status LED) (GPIO4) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_WS (Audio Word Select) (GPIO15) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_WS (Audio Word Select) (GPIO15) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_CLK (Audio Bit Clock) (GPIO16) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_CLK (Audio Bit Clock) (GPIO16) LOW <<<
I (xxx) gpio_test: >>> SETTING I2S_MICDATA (Audio Data) (GPIO18) HIGH <<<
I (xxx) gpio_test: >>> SETTING I2S_MICDATA (Audio Data) (GPIO18) LOW <<<
```

**已验证的引脚：**

| GPIO | 信号 | 预期行为 |
|------|-----------------|------------------------------------------------------|
| 8 | SDB | HIGH=~3.3V, LOW=~0V（启用/禁用 LED 驱动） |
| 4 | STATUS LED | HIGH=LED 亮, LOW=LED 灭 |
| 15 | I2S_WS | HIGH=~3.3V, LOW=~0V |
| 16 | I2S_CLK | HIGH=~3.3V, LOW=~0V |
| 18 | I2S_MICDATA | HIGH=~3.3V, LOW=~0V |
| 38 | BUTTON | 带下拉的输入；按下时为 HIGH |

**跳过的引脚：**
- GPIO1（I2C_SCL）和 GPIO2（I2C_SDA）：这些引脚需要 I2C 外设驱动，不作为 GPIO 输出测试。
- GPIO43（TXD0）和 GPIO44（RXD0）：UART0 调试控制台，由 USB 转 UART 桥接器驱动，不测试。

### 4.4 验证步骤

1. 将万用表置于直流电压模式。
2. 黑色表笔接 GND。
3. 对于上表中列出的每个引脚，在查看串口输出的同时将红色表笔接触对应引脚。
4. 确认电压与预期的 HIGH（~3.3V）和 LOW（~0V）状态一致。
5. 对于按键（GPIO38）：按下物理按键，观察串口输出从"released"变为"PRESSED"。

### 4.5 恢复 CMakeLists.txt

测试后将 SRCS 行恢复为原始内容。

---

## 5. 正常固件烧录

### 5.1 恢复标准配置

确保 `firmware/main/CMakeLists.txt` 包含原始 SRCS 行：

```cmake
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
```

### 5.2 构建并烧录

```bash
cd ~/260510_clock/firmware
. ~/.espressif/v5.5.4/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 5.3 首次设置（AP 模式）

首次启动且无已保存的 WiFi 凭据时：

1. 串口监控显示 `WiFi manager started`，随后创建 AP。
2. 在手机或电脑上扫描 WiFi 网络。找到 `Clock-Config-XXXX`。
3. 连接到此接入点，无需密码。
4. 打开浏览器并导航到 `http://192.168.4.1`。
5. 配置页面允许你：
   - 选择家庭 WiFi 网络（SSID）。
   - 输入 WiFi 密码。
   - 配置 MQTT 后端 URL（默认：`mqtt://192.168.1.100:1883`）。
6. 提交配置。
7. ESP32-S3 重启并尝试连接你的 WiFi。

### 5.4 后续启动

成功配置 WiFi 后：

1. 固件从 NVS 读取已保存的凭据。
2. WiFi 以 STA 模式连接。
3. 基于 IP 的地理定位确定时区。
4. NTP 同步系统时钟。
5. 显示屏显示当前时间。
6. MQTT 连接到已配置的后端以进行远程控制。

**注意：** WiFi 连接仅支持 2.4GHz 网络。ESP32-S3 不支持 5GHz 网络。

---

## 6. 硬件集成测试

需要创建一个综合硬件测试固件（`hw_test_all.c`），按顺序测试所有子系统。

### 6.1 创建测试文件

创建 `firmware/main/hw_test_all.c`，结构如下：

```c
/**
 * Hardware Integration Test - Desktop Clock
 * Tests all subsystems in sequence, requiring user confirmation at each step.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "pinmap.h"

static const char *TAG = "hw_test";

void app_main(void)
{
    printf("\n========================================\n");
    printf("  Hardware Integration Test\n");
    printf("========================================\n\n");

    // Test 1: Status LED
    printf("[1/7] Status LED (GPIO4)\n");
    printf("      The on-board LED should blink 3 times.\n");
    printf("      Confirm: LED blinks? (y/n, then press Enter)\n");
    // ... blink LED 3 times ...

    // Test 2: I2C Bus Scan
    printf("[2/7] I2C Bus Scan\n");
    printf("      Scanning main bus + 6 mux channels.\n");
    printf("      Expected: 8 devices (1xAHT21 + 1xTCA9548A + 6xIS31FL3729)\n");
    // ... run I2C scan from i2c_scan.c logic ...

    // Test 3: Button
    printf("[3/7] Button (GPIO38)\n");
    printf("      Press the button now.\n");
    printf("      Confirm: button press detected? (y/n, then press Enter)\n");
    // ... wait for button press ...

    // Test 4: LED Matrix All-On
    printf("[4/7] LED Matrix - All LEDs ON\n");
    printf("      All LED matrices should light up at full brightness.\n");
    printf("      Confirm: all matrices lit? (y/n, then press Enter)\n");
    // ... initialize LED drivers, set all LEDs on ...

    // Test 5: LED Matrix Ramp
    printf("[5/7] LED Matrix - Brightness Ramp\n");
    printf("      LEDs should smoothly ramp from 0 to 100%% and back.\n");
    printf("      Confirm: smooth ramp visible? (y/n, then press Enter)\n");

    // Test 6: LED Matrix Test Pattern
    printf("[6/7] LED Matrix - Test Pattern\n");
    printf("      Each matrix should display a unique test pattern.\n");
    printf("      Confirm: patterns visible on all matrices? (y/n, then press Enter)\n");

    // Test 7: AHT21 Sensor
    printf("[7/7] AHT21 Temperature/Humidity Sensor\n");
    printf("      Reading sensor values...\n");
    printf("      Temperature: XX.X C  Humidity: XX.X %%\n");
    printf("      Confirm: values are reasonable? (y/n, then press Enter)\n");

    printf("\n========================================\n");
    printf("  ALL TESTS COMPLETE\n");
    printf("========================================\n");
}
```

**注意：** 这是骨架代码。完整实现需要集成来自现有组件库的 LED 驱动初始化、I2C 扫描逻辑、AHT21 读取和按键检测。该文件在仓库中尚不存在。

### 6.2 构建并烧录

```bash
# 编辑 CMakeLists.txt: SRCS "hw_test_all.c"
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 6.3 逐项执行测试

按照每个提示执行。在每个测试步骤后，目视确认结果并响应（若已实现串口输入），或者通过观察确认（若测试仅为输出型）。

---

## 7. 故障排除

### 7.1 无串口输出

- **验证波特率：** ESP-IDF 监控器默认使用 115200。如果使用独立的串口程序，确保终端设置为 115200。
- **检查 USB 数据线：** 数据线必须是数据线而非纯充电线。通过检查连接时 `dmesg`（Linux）或设备管理器（Windows）是否显示新设备来验证。
- **检查端口：** 确保使用正确的端口。在 Linux 上，连接前后执行 `ls /dev/ttyUSB*`，新出现的条目即为你的设备。
- **验证设备已上电：** 板载电源 LED 应亮起。若不亮，检查 USB 连接并尝试其他端口。
- **尝试手动串口终端：** 直接使用 `screen`、`minicom` 或 `picocom`：
  ```bash
  screen /dev/ttyUSB0 115200
  # 退出 screen: Ctrl+A，然后 K，再 Y
  ```

### 7.2 I2C 扫描无设备

- **SDB 引脚：** 测量 GPIO8 电压。通道扫描期间必须为 ~3.3V。若引脚卡在 LOW，IS31FL3729 驱动保持关断状态，不在 I2C 上响应。
- **上拉电阻：** 验证 SDA（GPIO2）和 SCL（GPIO1）上安装了 2.2k 至 4.7k 的上拉电阻到 3.3V。无上拉电阻时 I2C 总线无法工作。
- **供电：** 验证 3.3V 电压轨稳定。I2C 设备需要清洁的电源。
- **焊点：** 在放大镜下检查所有 I2C 设备焊点。SDA 或 SCL 上一个虚焊就可能导致整个总线失效。

### 7.3 LED 矩阵不亮

- **GCC（全局电流控制）：** IS31FL3729 上电默认值可能将 GCC 设为极低值，使 LED 看似熄灭。`is31fl3729_init()` 中的初始化流程会将 GCC 设置到可见的水平。
- **SDB 引脚：** SDB 引脚（GPIO8）必须为 HIGH 以启用 LED 驱动。若为 LOW，所有驱动处于关断模式，全部 LED 输出为高阻态。
- **3729 初始化：** 验证在启动期间对 6 个通道均调用了 `is31fl3729_init()`。app_main.c 的初始化循环遍历通道 0 至 5。
- **TCA9548A 多路复用器：** 若多路复用器未正确初始化，则不会发生通道选择，无法寻址 IS31FL3729 驱动。在 I2C 扫描中验证 TCA9548A 出现在 0x70。

### 7.4 WiFi 连接失败

- **仅支持 2.4GHz：** ESP32-S3 仅支持 2.4GHz WiFi。设备无法看到 5GHz 网络。
- **SSID 包含特殊字符：** SSID 中的某些特殊字符或空格可能导致连接问题。先用简单的 ASCII SSID 测试。
- **信号强度：** 若设备在金属外壳内或距离接入点太远，信号可能不足。先靠近路由器测试。
- **已保存凭据损坏：** 若存储的 WiFi 凭据损坏，擦除 NVS：
  ```bash
  idf.py -p /dev/ttyUSB0 erase-flash
  idf.py -p /dev/ttyUSB0 flash monitor
  ```
  这将清除所有已保存的设置，强制下次启动时进入全新的 AP 模式设置。

### 7.5 烧录失败（下载模式）

若 `idf.py flash` 因连接错误或超时而失败：

1. **手动进入 bootloader：**
   - 按住 **BOOT** 按钮（多数 ESP32-S3 开发板上标注为 "BOOT" 或 "IO0"）。
   - 按住 BOOT 的同时，按下并释放 **RST**（或 "EN"）按钮。
   - 1-2 秒后释放 BOOT 按钮。
   - 此时芯片等待固件下载。
   - 重新执行 `idf.py -p /dev/ttyUSB0 flash`。
2. **替代方法：** 在整个烧录过程中按住 BOOT。仅在看到进度输出（"Writing at 0x..."）后释放。
3. **检查 USB 数据线：** 更换为已知可用的数据线。某些数据线仅供电。
4. **检查端口权限（Linux）：** 确保你的用户在 `dialout` 组中：
   ```bash
   sudo usermod -a -G dialout $USER
   # 注销后重新登录使更改生效
   ```

### 7.6 OTA 回滚

若 OTA 更新失败，bootloader 的回滚功能会恢复至上一可用的固件。若发生回滚，启动时的串口输出会显示 `ota_check_rollback()`。这是正常现象，表明系统正在自我恢复。

---

## 8. 引脚参考

来自 `firmware/components/config/include/pinmap.h` 的全部 GPIO 分配：

### I2C 总线

| 信号 | GPIO | 备注 |
|--------|------|------------------------------------------|
| SCL | 1 | I2C 时钟，2.2k-4.7k 上拉到 3.3V |
| SDA | 2 | I2C 数据，2.2k-4.7k 上拉到 3.3V |

### LED 矩阵控制

| 信号 | GPIO | 备注 |
|--------|------|----------------------------------------------------|
| SDB | 8 | 全部 6 个 IS31FL3729 驱动的关断控制（高电平有效） |

### 状态指示

| 信号 | GPIO | 备注 |
|-------------|------|------------------------------|
| STATUS LED | 4 | 板载指示灯 |

### 用户输入

| 信号 | GPIO | 备注 |
|--------|------|--------------------------------------|
| BUTTON | 38 | 高电平有效，外部下拉 |

### I2S 音频（Phase 2）

| 信号 | GPIO | 备注 |
|-------------|------|----------------|
| I2S_WS | 15 | 字选 |
| I2S_CLK | 16 | 位时钟 |
| I2S_MICDATA | 18 | 麦克风数据 |

### USB（原生）

| 信号 | GPIO | 备注 |
|--------|------|--------------------------------------------|
| D- | 19 | USB OTG 数据负（不作为 GPIO 使用） |
| D+ | 20 | USB OTG 数据正（不作为 GPIO 使用） |

### UART0（调试控制台）

| 信号 | GPIO | 备注 |
|--------|------|--------------------------------------------|
| TXD0 | 43 | UART 发送（连接到 USB 桥接器 TX） |
| RXD0 | 44 | UART 接收（连接到 USB 桥接器 RX） |

### 默认固件参数

| 参数 | 值 | 来源 |
|------------------------|-------------------------------|-------------------|
| 固件版本 | 0.1.0 | app_main.c:31 |
| 硬件版本 | 0.1 | app_main.c:32 |
| I2C 频率 | 400 kHz | app_main.c:30 |
| I2C 端口 | I2C_NUM_0 | app_main.c:29 |
| Flash 大小 | 16 MB | sdkconfig.defaults:35-36 |
| Factory 分区 | 3 MB (0x300000) | partitions.csv:5 |
| OTA 分区（×2） | 各 3 MB | partitions.csv:6-7 |
| SPIFFS 存储 | 2 MB (0x200000) | partitions.csv:9 |
| FreeRTOS 滴答频率 | 1000 Hz | sdkconfig.defaults:34 |
| 默认 MQTT Broker | mqtt://192.168.1.100:1883 | app_main.c:33 |
| WiFi AP SSID 模式 | Clock-Config-XXXX | wifi_mgr 组件 |

### 分区布局

```
Offset     Size       Name        Type     SubType
0x9000     0x6000     nvs         data     nvs
0xf000     0x2000     otadata     data     ota
0x11000    0x1000     phy_init    data     phy
0x20000    0x300000   factory     app      factory
(auto)     0x300000   ota_0       app      ota_0
(auto)     0x300000   ota_1       app      ota_1
(auto)     0x6000     nvs_user    data     nvs
(auto)     0x200000   storage     data     spiffs
```

---

## 快速命令参考

```bash
# 进入项目目录
cd ~/260510_clock/firmware

# 加载 ESP-IDF 环境
. ~/.espressif/v5.5.4/esp-idf/export.sh

# 设置目标（一次性）
idf.py set-target esp32s3

# 构建
idf.py build

# 烧录并监控
idf.py -p /dev/ttyUSB0 flash monitor

# 仅监控（不烧录）
idf.py -p /dev/ttyUSB0 monitor

# 擦除全部 flash
idf.py -p /dev/ttyUSB0 erase-flash

# 清理构建产物
idf.py fullclean

# 手动下载模式烧录（按住 BOOT，按 RST，释放 BOOT）
idf.py -p /dev/ttyUSB0 flash

# 退出串口监控
# 按下: Ctrl + ]
```
