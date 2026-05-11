#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c.h"
#include "pinmap.h"
#include "power_limit.h"
#include "framebuffer.h"
#include "display_task.h"
#include "wifi_mgr.h"
#include "geo_pipeline.h"
#include "card_mgr.h"
#include "clock_mqtt.h"
#include "config_sync.h"
#include "button.h"
#include "types.h"
#include "ota_handler.h"
#include "thin_http.h"
#include "is31fl3729.h"
#include "tca9548a.h"
#include "task_prio.h"

#define TAG "APP"
extern void render_task_func(void *pv);

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_FREQ_HZ         400000
#define FW_VERSION          "0.1.0"
#define HW_REV              "0.1"
#define MQTT_BROKER_DEFAULT "mqtt://192.168.1.100:1883"

static char s_mac_str[18];
static bool s_brightness_direction = true;
static QueueHandle_t s_button_queue = NULL;

static void get_mac_string(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(s_mac_str, sizeof(s_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device MAC: %s", s_mac_str);
}

static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "I2C initialized: SDA=IO%d SCL=IO%d %dHz",
             PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    return ESP_OK;
}

static void handle_button_event(button_event_t evt)
{
    switch (evt) {
    case BTN_EVENT_SINGLE_CLICK:
        card_mgr_toggle_display();
        ESP_LOGI(TAG, "Button: toggle display → %s",
                 card_mgr_is_display_on() ? "ON" : "OFF");
        break;

    case BTN_EVENT_DOUBLE_CLICK:
        card_next();
        ESP_LOGI(TAG, "Button: next card → %d/%d",
                 card_get_index(), card_get_count());
        break;

    case BTN_EVENT_LONG_PRESS_START:
        ESP_LOGI(TAG, "Button: long press start");
        break;

    case BTN_EVENT_LONG_PRESS_HOLD:
        brightness_adjust(s_brightness_direction);
        display_set_global_brightness(I2C_MASTER_NUM, brightness_get_gcc());
        ESP_LOGI(TAG, "Button: brightness adjust → %d", brightness_get_gcc());
        break;

    case BTN_EVENT_LONG_PRESS_RELEASE:
        s_brightness_direction = !s_brightness_direction;
        ESP_LOGI(TAG, "Button: long press release, direction=%s",
                 s_brightness_direction ? "up" : "down");
        break;

    default:
        break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Desktop Clock Firmware v%s ===", FW_VERSION);

    // 1. NVS must be first (required by WiFi, cards, config)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash needs erase, wiping...");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");

    // 2. Get device MAC (used by MQTT, config sync)
    get_mac_string();

    // 3. Hardware: I2C bus
    init_i2c();

    // 3a. Configure SDB GPIO as output (all 6 LED drivers share IO8)
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

    // 3d. Read backend URL from NVS (fallback to default)
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

    // 4. Display: framebuffer + display task (PRIO 5 — highest)
    framebuffer_init();
    err = display_task_create(I2C_MASTER_NUM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display task creation failed");
    }

    // 4a. Create render task (PRIO 4 — renders backgrounds and foregrounds)
    xTaskCreate(render_task_func, "render", STACK_RENDER, NULL, PRIO_RENDER, NULL);
    ESP_LOGI(TAG, "Render task created");

    // 5. Card manager (loads from NVS, sets default if empty)
    card_mgr_init();

    // 6. WiFi (reads saved credentials, attempts connect; falls back to AP)
    wifi_mgr_init();
    ESP_LOGI(TAG, "WiFi manager started");

    // 7. Wait for WiFi to connect or fail, then geo+NTP auto sync
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected — starting geo+NTP sync");
        geo_auto_sync();

        // OTA rollback check (safe no-op on factory/first boot)
        ota_check_rollback();
    } else {
        ESP_LOGW(TAG, "WiFi connection failed — running in offline mode");
    }

    // 8. MQTT client (needs WiFi, uses MAC for client_id)
    mqtt_set_mac(s_mac_str);
    err = mqtt_client_init(backend_url);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MQTT init failed, entering thin mode");
        thin_http_start();
    }

    // 9. Config sync (registers MQTT callback for remote config push)
    config_sync_init(s_mac_str);

    // 10. Button (GPIO interrupt → event queue)
    s_button_queue = button_init();
    ESP_LOGI(TAG, "Button initialized");

    // 11. Publish initial status if connected
    if (mqtt_is_connected()) {
        mqtt_publish_status(FW_VERSION, HW_REV, "0.0.0.0", "");
        config_sync_report_to_server();
    }

    // 12. Main loop: process button events + periodic check
    ESP_LOGI(TAG, "Entering main loop");
    button_event_t evt;
    while (1) {
        if (xQueueReceive(s_button_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            handle_button_event(evt);
        }
    }
}
