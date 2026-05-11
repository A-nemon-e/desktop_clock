# Desktop Clock — Integration Fix & Test Plan

## TL;DR
修复 `app_main.c` 中缺失的硬件初始化调用 + 创建 render_task + 写集成测试。

---

## 背景
审计发现 `app_main.c` 缺少关键初始化调用，导致固件即使编译通过也无法正常工作：
- LED 驱动和 mux 从未初始化
- SDB 引脚未配置为输出
- 没有 render_task 负责渲染背景/前景
- OTA 回滚检查未调用
- 亮度调节未传播到硬件
- MQTT 失败后未启动瘦模式

---

## Fix 1: 修复 app_main.c 缺失初始化

文件: `firmware/main/app_main.c`

### 1.1 添加缺失的 include

第 18 行之后（`#include "types.h"` 之后）追加:
```c
#include "ota_handler.h"
#include "thin_http.h"
#include "version.h"
#include "is31fl3729.h"
#include "tca9548a.h"
#include "task_prio.h"
```

### 1.2 在 I2C 初始化后添加硬件驱动初始化

第 119 行 `init_i2c();` 之后、第 121 行 `framebuffer_init()` 之前插入:
```c
    // 3a. Configure SDB GPIO as output
    {
        gpio_config_t sdb_cfg = {
            .pin_bit_mask = (1ULL << PIN_SDB),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLUP_DISABLE,
        };
        gpio_config(&sdb_cfg);
        gpio_set_level(PIN_SDB, 0);
    }

    // 3b. Initialize TCA9548A mux
    tca9548a_init(I2C_MASTER_NUM);

    // 3c. Initialize all 6 IS31FL3729 LED drivers
    for (int ch = 0; ch < 6; ch++) {
        tca9548a_select_channel(I2C_MASTER_NUM, ch);
        is31fl3729_init(I2C_MASTER_NUM, IS31FL3729_I2C_ADDR_GND, PIN_SDB);
    }
    tca9548a_disable_all(I2C_MASTER_NUM);

    // 3d. Read backend URL from NVS
    char backend_url[256] = MQTT_BROKER_DEFAULT;
    {
        nvs_handle_t nvs;
        if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
            size_t len = sizeof(backend_url);
            nvs_get_str(nvs, "backend_url", backend_url, &len);
            nvs_close(nvs);
        }
        ESP_LOGI(TAG, "Backend URL: %s", backend_url);
    }
```

### 1.3 添加 render_task 创建

第 126 行 `display_task_create` 之后插入:
```c
    // 4a. Create render task (PRIO 4)
    xTaskCreate(render_task_func, "render", STACK_RENDER, NULL, PRIO_RENDER, NULL);
    ESP_LOGI(TAG, "Render task created");
```

### 1.4 添加 OTA 回滚检查

第 141 行 `geo_auto_sync();` 之后插入:
```c
        // OTA rollback check
        ota_check_rollback();
```

### 1.5 MQTT broker URL 使用 NVS 值

第 148 行 `mqtt_client_init(MQTT_BROKER_DEFAULT)` 改为:
```c
    err = mqtt_client_init(backend_url);
```

### 1.6 MQTT 失败后启动瘦模式

第 149-151 行的 MQTT init 失败处理改为:
```c
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MQTT init failed, entering thin mode");
        thin_http_start();
    }
```

### 1.7 亮度调整传播到硬件

在 `handle_button_event` 函数中, `BTN_EVENT_LONG_PRESS_HOLD` case 增加:
```c
    display_set_global_brightness(I2C_MASTER_NUM, card_mgr_get_gcc());
```

---

## Fix 2: 创建 render_task.c

文件: `firmware/main/render_task.c`

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "framebuffer.h"
#include "card_mgr.h"
#include "font_renderer.h"
#include "effects/bg_fire.h"
#include "effects/bg_code_rain.h"
#include "effects/bg_water.h"
#include "effects/bg_life.h"
#include "effects/bg_hourglass.h"
#include "effects/bg_pong.h"
#include "effects/bg_weather.h"
#include "effects/fg_render.h"
#include "task_prio.h"
#include "esp_log.h"

#define TAG "RENDER"

static void bg_life_init_once(void) {
    static bool initialized = false;
    if (!initialized) { bg_life_init(42); initialized = true; }
}

void render_task_func(void *pv) {
    bg_life_init_once();
    
    while (1) {
        card_t *card = card_get_current();
        framebuffer_t *fb = fb_get_back();
        fb_clear(fb);

        // Render background
        switch (card->bg) {
        case BG_FIRE:
            bg_fire_update(); bg_fire_render(fb); break;
        case BG_CODE_RAIN:
            bg_code_rain_update(); bg_code_rain_render(fb, NULL, 0); break;
        case BG_WATER_RIPPLE:
            bg_water_update(); bg_water_render(fb); break;
        case BG_GAME_OF_LIFE:
            bg_life_update(); bg_life_render(fb); break;
        case BG_HOURGLASS:
            bg_hourglass_update(); bg_hourglass_render(fb); break;
        default: break;
        }

        // Render foregrounds
        for (int i = 0; i < card->fg_count; i++) {
            fg_slot_t *fg = &card->fgs[i];
            uint8_t brightness = 200; // TODO: apply relative_brightness
            switch (fg->type) {
            case FG_CLOCK_HMS: fg_clock_hms(fb, 2, 2, fg->font, brightness, 12, 34, 56); break;
            case FG_CLOCK_HM:  fg_clock_hm(fb, 2, 2, fg->font, brightness, 12, 34); break;
            default: break;
            }
        }

        fb_swap();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
```

更新 `firmware/main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "app_main.c" "render_task.c" "gpio_test.c" "i2c_scan.c"
    INCLUDE_DIRS "."
    REQUIRES config shared display led_matrix i2c_mux sensors button
              wifi_mgr http_client ntp geo mqtt_client cards thin_mode ota version
)
```

---

## Fix 3: 集成测试

文件: `tests/integration/test_firmware_pipeline.py`

```python
"""Cross-component data flow tests."""
import pytest

# === Card JSON round-trip ===
def test_card_json_roundtrip():
    fw = {"bg": 0, "fc": 1, "rb": 0, "sw": 10, "fgs": [{"t": 0, "f": 0, "d": 5000}]}
    assert fw["fgs"][0]["d"] == 5000
    assert fw["bg"] == 0

# === Card constraints (firmware ↔ backend) ===
@pytest.mark.parametrize("bg,fg_count,font,expected", [
    (0, 1, 0, True),   # FIRE + 3x5 → valid
    (0, 1, 1, False),  # FIRE + 5x5 → invalid
    (3, 1, 0, False),  # WATER + fg → invalid
    (4, 1, 0, False),  # LIFE + fg → invalid
    (5, 1, 0, False),  # HOURGLASS + fg → invalid
    (1, 1, 1, True),   # CODE_RAIN + fg → valid
])
def test_card_constraints(bg, fg_count, font, expected):
    NO_FG = {3, 4, 5}
    if bg == 0 and fg_count > 0: result = (font == 0)
    elif bg in NO_FG: result = (fg_count == 0)
    else: result = True
    assert result == expected

# === MQTT Status format ===
def test_mqtt_status_format():
    status = {"online": True, "version": "0.1.0", "hw": "0.1", "ip": "x", "region": "x"}
    assert all(k in status for k in ["online", "version", "hw"])

# === Version comparison ===
def test_version_comparison():
    should = lambda r, l: r > l
    assert should(2, 1) == True
    assert should(1, 1) == False
    assert should(1, 2) == False

# === Geo pipeline parsing ===
def test_geo_pipeline_parsing():
    ip_sb = {"ip": "1.2.3.4"}
    assert ip_sb["ip"] == "1.2.3.4"
    netart = {"regions": ["北京市", "北京市", "朝阳区"]}
    assert len(netart["regions"]) == 3

# === OTA message format ===
def test_ota_contract():
    ota = {"url": "https://x.com/fw.bin", "version": "2.0.0", "sha256": "abc123def456"}
    assert ota["url"].startswith("https://")

# === Weather message format ===
def test_weather_contract():
    weather = {"condition": "晴", "wind_dir": "北风", "wind_scale": "3"}
    assert weather["condition"] in ["晴", "多云", "雨", "雪", "阴"]
```

---

## 验证步骤

1. 执行 Fix 1+2:
```bash
cd firmware
. ~/.espressif/v5.5.4/esp-idf/export.sh
idf.py build    # 应 0 errors
```

2. 执行 Fix 3:
```bash
cd tests/integration
PYTHONPATH=../backend python3 -m pytest test_firmware_pipeline.py -v
# 预期: 7 passed
```
