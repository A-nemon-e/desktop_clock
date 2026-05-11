#include "tca9548a.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TCA9548";

esp_err_t tca9548a_init(uint8_t i2c_bus)
{
    // RESET is hardware pull-up — no GPIO control needed
    // Disable all channels on init
    return tca9548a_disable_all(i2c_bus);
}

esp_err_t tca9548a_select_channel(uint8_t i2c_bus, uint8_t channel)
{
    if (channel >= TCA9548A_CHANNELS) {
        ESP_LOGE(TAG, "Invalid channel: %d (max %d)", channel, TCA9548A_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t control_byte = (1 << channel);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9548A_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, control_byte, true);
    i2c_master_stop(cmd);  // STOP condition activates the selected channel

    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select channel %d: %d", channel, ret);
    }

    return ret;
}

esp_err_t tca9548a_disable_all(uint8_t i2c_bus)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9548A_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);  // All channels off
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable all channels: %d", ret);
    }

    return ret;
}
