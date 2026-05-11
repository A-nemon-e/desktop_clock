#include "is31fl3729.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IS31FL";

// Helper: write single register
static esp_err_t write_reg(uint8_t i2c_bus, uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Helper: burst write multiple bytes starting from register
static esp_err_t burst_write(uint8_t i2c_bus, uint8_t addr, uint8_t start_reg,
                              const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, start_reg, true);
    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t is31fl3729_init(uint8_t i2c_bus, uint8_t addr, int sdb_pin)
{
    // 1. Pull SDB high (all 6 drivers share IO8)
    gpio_set_level(sdb_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 2. Write CONFIG = normal operation
    esp_err_t ret = write_reg(i2c_bus, addr, IS31FL3729_REG_CONFIG, IS31FL3729_CONFIG_NORMAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set config at addr 0x%02X: %d", addr, ret);
        return ret;
    }

    // 3. Set GCC = max brightness (subsequently limited by power_limit.h)
    ret = is31fl3729_set_gcc(i2c_bus, addr, IS31FL3729_GCC_MAX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GCC at addr 0x%02X: %d", addr, ret);
        return ret;
    }

    // 4. Clear all PWM registers (all LEDs off)
    uint8_t zeros[IS31FL3729_LED_COUNT] = {0};
    ret = is31fl3729_update_pwm(i2c_bus, addr, zeros);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear PWM at addr 0x%02X: %d", addr, ret);
        return ret;
    }

    ESP_LOGI(TAG, "Initialized driver at 0x%02X", addr);
    return ESP_OK;
}

esp_err_t is31fl3729_set_gcc(uint8_t i2c_bus, uint8_t addr, uint8_t gcc)
{
    if (gcc > IS31FL3729_GCC_MAX) gcc = IS31FL3729_GCC_MAX;
    return write_reg(i2c_bus, addr, IS31FL3729_REG_GCC, gcc);
}

esp_err_t is31fl3729_update_pwm(uint8_t i2c_bus, uint8_t addr, const uint8_t *pwm_data)
{
    // Burst write 128 bytes from REG_PWM (0x01) — auto-increment handles rest
    return burst_write(i2c_bus, addr, IS31FL3729_REG_PWM, pwm_data, IS31FL3729_LED_COUNT);
}

esp_err_t is31fl3729_shutdown(uint8_t i2c_bus, uint8_t addr)
{
    return write_reg(i2c_bus, addr, IS31FL3729_REG_CONFIG, IS31FL3729_CONFIG_SHUTDOWN);
}

esp_err_t is31fl3729_reset(uint8_t i2c_bus, uint8_t addr)
{
    return write_reg(i2c_bus, addr, IS31FL3729_REG_RESET, 0x00);
}
