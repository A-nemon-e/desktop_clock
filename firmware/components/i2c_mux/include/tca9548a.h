#pragma once
#include <stdint.h>
#include "esp_err.h"

// TCA9548A I2C address (A0=A1=A2=GND)
#define TCA9548A_I2C_ADDR  0x70

// Number of available channels
#define TCA9548A_CHANNELS  8

/**
 * Initialize the TCA9548A multiplexer.
 * RESET is hardware pull-up (no GPIO control needed).
 * Disables all channels on init.
 * 
 * @param i2c_bus  I2C bus handle (I2C_NUM_0)
 * @return ESP_OK on success
 */
esp_err_t tca9548a_init(uint8_t i2c_bus);

/**
 * Select a single channel (disables all others).
 * Channel becomes active after I2C STOP condition.
 * 
 * @param i2c_bus  I2C bus handle
 * @param channel  Channel number (0-7)
 * @return ESP_OK on success
 */
esp_err_t tca9548a_select_channel(uint8_t i2c_bus, uint8_t channel);

/**
 * Disable all channels (no device selected).
 * 
 * @param i2c_bus  I2C bus handle
 * @return ESP_OK on success
 */
esp_err_t tca9548a_disable_all(uint8_t i2c_bus);
