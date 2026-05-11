#pragma once
#include "esp_err.h"

/**
 * Create the high-priority display refresh task.
 * Runs at PRIO_DISPLAY (5) — highest task priority.
 * Reads from fb_front and writes to IS31FL3729 drivers via TCA9548A mux.
 *
 * IMPORTANT: This task does NO network/sensor I/O.
 * It only reads the framebuffer and drives I2C to the LED matrix.
 *
 * @param i2c_bus  I2C bus handle (I2C_NUM_0)
 * @return ESP_OK on success
 */
esp_err_t display_task_create(uint8_t i2c_bus);

/**
 * Set global brightness for all 6 LED drivers.
 * Clamped to range [5, IS31FL3729_GCC_MAX].
 *
 * NOTE: Must be called from task context, NOT from ISR.
 * For brightness changes triggered by button events, route through
 * a queue so the display_task or its own context calls this function.
 *
 * @param i2c_bus  I2C bus handle (I2C_NUM_0)
 * @param gcc      Global Current Control value (0-127)
 * @return ESP_OK on success, ESP_FAIL if any driver write fails
 */
esp_err_t display_set_global_brightness(uint8_t i2c_bus, uint8_t gcc);
