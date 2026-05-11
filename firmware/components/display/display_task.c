#include "display_task.h"
#include "framebuffer.h"
#include "task_prio.h"
#include "pinmap.h"
#include "is31fl3729.h"
#include "tca9548a.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DSP";

#define DISPLAY_FPS         30
#define FRAME_PERIOD_MS     (1000 / DISPLAY_FPS)  // ~33ms

// I2C bus handle (set on task creation)
static uint8_t s_i2c_bus = 0;

// IS31FL3729 addresses for each mux channel (all share 0x34 since behind mux)
#define LED_DRIVER_ADDR IS31FL3729_I2C_ADDR_GND  // 0x34

// Current global brightness (modified by button long-press)
static uint8_t s_gcc_value = 20;  // Start at safe default

static void display_task_func(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t frame_count = 0;
    int64_t last_fps_log = 0;

    ESP_LOGI(TAG, "Display task started (target: %d FPS, period: %d ms)",
             DISPLAY_FPS, FRAME_PERIOD_MS);

    while (1) {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(FRAME_PERIOD_MS));
        frame_count++;

        // Swap framebuffers (fb_swap handles mutex internally)
        fb_swap();

        // Drive all 6 LED drivers through TCA9548A mux
        for (int ch = 0; ch < 6; ch++) {
            esp_err_t ret = tca9548a_select_channel(s_i2c_bus, ch);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to select mux ch %d", ch);
                continue;
            }

            // Build PWM data for this driver's 8 columns × 16 rows = 128 LEDs
            uint8_t pwm_data[IS31FL3729_LED_COUNT];
            int col_offset = ch * 8;  // Columns 0-7, 8-15, ..., 40-47

            for (int led = 0; led < IS31FL3729_LED_COUNT; led++) {
                int row = led / 8;    // 0-15
                int col = led % 8;    // 0-7
                pwm_data[led] = fb_front.pixels[row][col_offset + col];
            }

            // Burst write all 128 PWM values via I2C auto-increment
            ret = is31fl3729_update_pwm(s_i2c_bus, LED_DRIVER_ADDR, pwm_data);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to update PWM on ch %d", ch);
            }
        }

        // Disable all mux channels after frame
        tca9548a_disable_all(s_i2c_bus);

        // Log FPS every 5 seconds
        int64_t now = esp_timer_get_time();
        if (now - last_fps_log > 5000000) {
            float actual_fps = (float)frame_count / ((now - last_fps_log) / 1000000.0f);
            ESP_LOGI(TAG, "FPS: %.1f (target: %d)", actual_fps, DISPLAY_FPS);
            frame_count = 0;
            last_fps_log = now;
        }
    }
}

esp_err_t display_task_create(uint8_t i2c_bus)
{
    s_i2c_bus = i2c_bus;

    BaseType_t ret = xTaskCreate(
        display_task_func,
        "display",
        STACK_DISPLAY,
        NULL,
        PRIO_DISPLAY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t display_set_global_brightness(uint8_t i2c_bus, uint8_t gcc)
{
    if (gcc > IS31FL3729_GCC_MAX) {
        gcc = IS31FL3729_GCC_MAX;
    }
    if (gcc < IS31FL3729_GCC_MIN) {
        gcc = IS31FL3729_GCC_MIN;
    }

    s_gcc_value = gcc;

    esp_err_t ret = ESP_OK;
    for (int ch = 0; ch < 6; ch++) {
        tca9548a_select_channel(i2c_bus, ch);
        esp_err_t r = is31fl3729_set_gcc(i2c_bus, LED_DRIVER_ADDR, gcc);
        if (r != ESP_OK) {
            ret = r;
        }
    }
    tca9548a_disable_all(i2c_bus);

    ESP_LOGI(TAG, "Brightness set: GCC=%d/%d", gcc, IS31FL3729_GCC_MAX);
    return ret;
}
