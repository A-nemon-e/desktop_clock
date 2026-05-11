#include "version.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "VER";

#ifndef FW_VERSION
#define FW_VERSION "1.0.0"
#endif

#define TCA9548A_I2C_ADDR  0x70
#define I2C_BUS             I2C_NUM_0
#define I2C_PROBE_TIMEOUT_MS 20

static char hw_revision[8] = {0};

const char* get_fw_version(void)
{
    return FW_VERSION;
}

/**
 * Probe an I2C address: send START → address+W → STOP.
 * Returns true if device ACKs.
 */
static bool i2c_probe_address(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_BUS, cmd, pdMS_TO_TICKS(I2C_PROBE_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return (ret == ESP_OK);
}

const char* get_hw_revision(void)
{
    if (hw_revision[0] != '\0') {
        return hw_revision;
    }

    ESP_LOGI(TAG, "Auto-detecting hardware revision...");

    bool mux_found = false;
    for (uint8_t addr = 0x70; addr <= 0x77; addr++) {
        if (i2c_probe_address(addr)) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
            if (addr == TCA9548A_I2C_ADDR) {
                mux_found = true;
            }
        }
    }

    if (mux_found) {
        ESP_LOGI(TAG, "TCA9548A detected → HW Rev 0.1");
        strncpy(hw_revision, "0.1", sizeof(hw_revision) - 1);
    } else {
        ESP_LOGI(TAG, "No TCA9548A detected → HW Rev 0.2");
        strncpy(hw_revision, "0.2", sizeof(hw_revision) - 1);
    }

    return hw_revision;
}
