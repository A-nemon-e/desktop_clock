/**
 * @file gpio_test.c
 * @brief GPIO test firmware - cycles through GPIOs HIGH/LOW for multimeter verification
 *
 * WARNING: This is a TEST firmware. Do NOT use in production.
 * I2C pins (GPIO1, GPIO2) are NOT driven as outputs — they require I2C peripheral.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "pinmap.h"

static const char *TAG = "gpio_test";

// List of GPIOs to test (excluding I2C pins which need peripheral)
// Each entry: { pin_number, pin_name_string }
typedef struct {
    gpio_num_t pin;
    const char *name;
} gpio_test_pin_t;

static const gpio_test_pin_t test_pins[] = {
    { PIN_SDB,         "SDB (IS31FL3729 Shutdown)"     },
    { PIN_LED,         "LED (Status LED)"               },
    { PIN_I2S_WS,      "I2S_WS (Audio Word Select)"     },
    { PIN_I2S_CLK,     "I2S_CLK (Audio Bit Clock)"      },
    { PIN_I2S_MICDATA, "I2S_MICDATA (Audio Data)"       },
};

static const int num_test_pins = sizeof(test_pins) / sizeof(test_pins[0]);

void app_main(void)
{
    ESP_LOGI(TAG, "GPIO Test Firmware Starting...");
    ESP_LOGI(TAG, "Pinmap version - I2C: SCL=GPIO1, SDA=GPIO2");
    ESP_LOGI(TAG, "WARNING: I2C pins skipped (need I2C peripheral)");
    printf("\n");

    // --- Configure test GPIOs as outputs ---
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 0,
        .pull_down_en = 0,
        .pull_up_en   = 0,
    };

    for (int i = 0; i < num_test_pins; i++) {
        io_conf.pin_bit_mask |= (1ULL << test_pins[i].pin);
    }
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // --- Configure button as input with pull-down ---
    gpio_config_t btn_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_BUTTON),
        .pull_down_en = 1,
        .pull_up_en   = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));
    ESP_LOGI(TAG, "Button (GPIO38) configured as input with pull-down");
    printf("\n");

    // --- Main test loop ---
    int cycle = 0;
    while (1) {
        cycle++;
        ESP_LOGI(TAG, "=== Test Cycle %d ===", cycle);

        // Read and report button state
        int btn_state = gpio_get_level(PIN_BUTTON);
        ESP_LOGI(TAG, "Button state: %s", btn_state ? "PRESSED" : "released");

        // Skip I2C pins — print confirmation
        ESP_LOGI(TAG, "I2C_SCL (GPIO1): skipping (I2C peripheral)");
        ESP_LOGI(TAG, "I2C_SDA (GPIO2): skipping (I2C peripheral)");

        // Cycle through each test pin
        for (int i = 0; i < num_test_pins; i++) {
            ESP_LOGI(TAG, ">>> SETTING %s (GPIO%d) HIGH <<<",
                     test_pins[i].name, test_pins[i].pin);
            ESP_ERROR_CHECK(gpio_set_level(test_pins[i].pin, 1));
            vTaskDelay(pdMS_TO_TICKS(1000));

            ESP_LOGI(TAG, ">>> SETTING %s (GPIO%d) LOW <<<",
                     test_pins[i].name, test_pins[i].pin);
            ESP_ERROR_CHECK(gpio_set_level(test_pins[i].pin, 0));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        printf("\n");
    }
}
