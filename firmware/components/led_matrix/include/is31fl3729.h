#pragma once
#include <stdint.h>
#include "esp_err.h"

// ============================================================
// IS31FL3729 Register Map (verified against QMK production driver)
// ============================================================

// I2C address (AD0=AD1=GND)
#define IS31FL3729_I2C_ADDR_GND    0x34

// Register addresses
#define IS31FL3729_REG_PWM         0x01   // PWM registers: 0x01-0x8E (142 addresses, 128 used)
#define IS31FL3729_REG_SCALING     0x90   // Scaling control: 0x90-0x9F
#define IS31FL3729_REG_CONFIG      0xA0   // Configuration register
#define IS31FL3729_REG_GCC         0xA1   // Global Current Control (0-127)
#define IS31FL3729_REG_PULLDOWNUP  0xB0   // Pull-up/down resistor config
#define IS31FL3729_REG_SPREAD      0xB1   // Spread spectrum config
#define IS31FL3729_REG_PWM_FREQ    0xB2   // PWM frequency setting
#define IS31FL3729_REG_RESET       0xCF   // Soft reset

// Configuration register bits
#define IS31FL3729_CONFIG_NORMAL   0x01   // Normal operation mode
#define IS31FL3729_CONFIG_SHUTDOWN 0x00   // Software shutdown

// LED matrix dimensions
#define IS31FL3729_LED_COUNT       128    // 16 rows x 8 columns

// GCC range
#define IS31FL3729_GCC_MAX         0x7F   // 127 = max brightness
#define IS31FL3729_GCC_MIN         0x01   // 1 = minimum visible

// ============================================================
// Public API
// ============================================================

/**
 * Initialize one IS31FL3729 driver.
 * @param i2c_bus   I2C bus handle (I2C_NUM_0)
 * @param addr      I2C address (typically IS31FL3729_I2C_ADDR_GND)
 * @param sdb_pin   GPIO pin for SDB (all drivers share IO8)
 * @return ESP_OK on success
 */
esp_err_t is31fl3729_init(uint8_t i2c_bus, uint8_t addr, int sdb_pin);

/**
 * Set global current control (brightness) for one driver.
 * @param gcc  0-127 brightness level
 */
esp_err_t is31fl3729_set_gcc(uint8_t i2c_bus, uint8_t addr, uint8_t gcc);

/**
 * Update all 128 LED PWM values via single I2C burst write.
 * Writes from REG_PWM (0x01) auto-incrementing through all 128 values.
 * Hardware auto-latches new PWM values on I2C STOP.
 *
 * @param pwm_data  Array of 128 uint8_t values (0=off, 255=max)
 *                  Layout: [row0_col0..row0_col7, row1_col0..row1_col7, ...]
 */
esp_err_t is31fl3729_update_pwm(uint8_t i2c_bus, uint8_t addr, const uint8_t *pwm_data);

/**
 * Software shutdown (low power).
 */
esp_err_t is31fl3729_shutdown(uint8_t i2c_bus, uint8_t addr);

/**
 * Software reset.
 */
esp_err_t is31fl3729_reset(uint8_t i2c_bus, uint8_t addr);
