/**
 * I2C Bus Scanner for Desktop Clock Hardware Verification
 * 
 * Task: 0.2 — I2C Signal Integrity Verification
 * 
 * Scans:
 *   1. Main I2C bus (direct): 
 *      - 0x38 = AHT21 temperature/humidity sensor
 *      - 0x70 = TCA9548A I2C multiplexer (A0=A1=A2=GND)
 *   2. Through TCA9548A channels 0-5:
 *      - 0x34 = IS31FL3729 LED driver (AD0=AD1=GND) on each channel
 * 
 * Test sequence:
 *   - SDB=LOW: only AHT21 + TCA9548A visible (LED drivers disabled)
 *   - SDB=HIGH: mux channels accessible
 *   - Scan all 6 mux channels for IS31FL3729 at 0x34
 *   - Repeat at 400kHz for high-speed verification
 * 
 * Expected: 1 AHT21 + 1 TCA9548A + 6 IS31FL3729 = 8 devices total
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCAN";

#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000  // Start at 100kHz, try 400kHz after
#define I2C_MASTER_SDA_IO  2
#define I2C_MASTER_SCL_IO  1
#define TCA9548A_ADDR      0x70

#define PIN_SDB            GPIO_NUM_8

// Initialize I2C master
static esp_err_t i2c_master_init(uint32_t freq_hz)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = freq_hz,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Scan I2C bus for devices and print found addresses
static void scan_i2c_bus(const char *label)
{
    printf("\n=== %s ===\n", label);
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            printf("  Found device at 0x%02X", addr);
            if (addr == 0x38) printf(" [AHT21]");
            else if (addr == 0x70) printf(" [TCA9548A]");
            else if (addr == 0x34) printf(" [IS31FL3729]");
            printf("\n");
            count++;
        }
    }
    printf("  Total: %d device(s)\n", count);
}

// Select a single TCA9548A channel (disable all others)
static void mux_select(uint8_t ch)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9548A_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 1 << ch, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select mux channel %d", ch);
    }
    i2c_cmd_link_delete(cmd);
}

void app_main(void)
{
    // --- Init SDB pin (pull high to enable all LED drivers) ---
    gpio_config_t sdb_conf = {
        .pin_bit_mask = (1ULL << PIN_SDB),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&sdb_conf);

    printf("\n========================================\n");
    printf("  I2C Bus Verification - Desktop Clock\n");
    printf("========================================\n");

    // -------------------------------------------------------
    // Test 1: SDB LOW — only main-bus devices should respond
    // -------------------------------------------------------
    printf("\n--- Test 1: SDB=LOW (all LED drivers disabled) ---\n");
    gpio_set_level(PIN_SDB, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_master_init(100000);
    scan_i2c_bus("Main Bus (SDB=LOW)");
    // Expected: 0x38 (AHT21), 0x70 (TCA9548A) only
    // NO 0x34 devices visible

    // -------------------------------------------------------
    // Test 2: SDB HIGH — enable LED drivers
    // -------------------------------------------------------
    printf("\n--- Test 2: SDB=HIGH (all LED drivers enabled) ---\n");
    gpio_set_level(PIN_SDB, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    scan_i2c_bus("Main Bus (SDB=HIGH)");
    // 0x34 is behind the mux, should NOT appear on main bus directly

    // -------------------------------------------------------
    // Test 3: Scan through each TCA9548A channel
    // -------------------------------------------------------
    printf("\n--- Test 3: Through TCA9548A channels (ch0-ch5) ---\n");
    for (int ch = 0; ch < 6; ch++) {
        char label[32];
        snprintf(label, sizeof(label), "TCA9548A Channel %d", ch);
        mux_select(ch);
        vTaskDelay(pdMS_TO_TICKS(1));
        scan_i2c_bus(label);
        // Expected: 0x34 on each channel (one IS31FL3729 per channel)
    }

    // Disconnect from channel (safety)
    mux_select(0);

    // -------------------------------------------------------
    // Test 4: Repeat scan at 400kHz
    // -------------------------------------------------------
    printf("\n--- Test 4: 400kHz I2C scan (first 2 channels) ---\n");
    i2c_driver_delete(I2C_MASTER_NUM);
    i2c_master_init(400000);
    for (int ch = 0; ch < 2; ch++) {
        char label[32];
        snprintf(label, sizeof(label), "Channel %d @ 400kHz", ch);
        mux_select(ch);
        vTaskDelay(pdMS_TO_TICKS(1));
        scan_i2c_bus(label);
    }

    // --- Summary ---
    printf("\n========================================\n");
    printf("  Verification Complete\n");
    printf("========================================\n");
    printf("Expected: 1x AHT21 (0x38) + 1x TCA9548A (0x70)");
    printf(" + 6x IS31FL3729 (0x34 on ch0-5)\n");
    printf("Check:  all 8 devices found?\n");
    printf("Check:  SDB control works?\n");
    printf("Check:  400kHz scan OK?\n");
}
