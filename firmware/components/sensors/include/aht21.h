#pragma once
#include <stdint.h>
#include "esp_err.h"

// I2C address (fixed, not configurable)
#define AHT21_I2C_ADDR  0x38

// Measurement timing
#define AHT21_MEASURE_DELAY_MS  75   // Wait after trigger before reading

/**
 * Initialize AHT21 sensor (soft reset + calibration check).
 * Must be called once before reading.
 * 
 * @param i2c_bus  I2C bus handle (I2C_NUM_0)
 * @return ESP_OK on success
 */
esp_err_t aht21_init(uint8_t i2c_bus);

/**
 * Read temperature and humidity from AHT21.
 * Blocks for ~75ms while measurement completes.
 * DO NOT call from display_task!
 * 
 * @param i2c_bus       I2C bus handle
 * @param temperature   Output: temperature in Celsius
 * @param humidity      Output: relative humidity in percent (0-100)
 * @return ESP_OK on success
 */
esp_err_t aht21_read(uint8_t i2c_bus, float *temperature, float *humidity);

/**
 * Trigger measurement only (non-blocking).
 * Call aht21_read_result after 75ms to get values.
 */
esp_err_t aht21_trigger(uint8_t i2c_bus);

/**
 * Read result after trigger (non-blocking, call only after 75ms delay).
 */
esp_err_t aht21_read_result(uint8_t i2c_bus, float *temperature, float *humidity);
