#include "aht21.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AHT21";

// AHT21 commands
#define AHT21_CMD_SOFT_RESET    0xBA
#define AHT21_CMD_INIT          0xE1
#define AHT21_CMD_MEASURE       0xAC

// Calibration init bytes
#define AHT21_INIT_BYTE1        0x08
#define AHT21_INIT_BYTE2        0x00

// Measurement trigger bytes
#define AHT21_MEASURE_BYTE1     0x33
#define AHT21_MEASURE_BYTE2     0x00

// Status register bit for calibration
#define AHT21_STATUS_CALIBRATED 0x08  // Bit 3 = 1 means calibrated

esp_err_t aht21_init(uint8_t i2c_bus)
{
    esp_err_t ret;

    // 1. Soft reset
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT21_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, AHT21_CMD_SOFT_RESET, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed: %d", ret);
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2. Initialize/calibrate
    uint8_t init_data[] = {AHT21_CMD_INIT, AHT21_INIT_BYTE1, AHT21_INIT_BYTE2};
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT21_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, init_data[0], true);
    i2c_master_write_byte(cmd, init_data[1], true);
    i2c_master_write_byte(cmd, init_data[2], true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init/calibrate failed: %d", ret);
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    // 3. Read status to verify calibration
    uint8_t status;
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT21_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &status, I2C_MASTER_ACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Status read failed: %d", ret);
        return ret;
    }

    if (!(status & AHT21_STATUS_CALIBRATED)) {
        ESP_LOGW(TAG, "Sensor not calibrated (status=0x%02X)", status);
    }

    ESP_LOGI(TAG, "Initialized (status=0x%02X)", status);
    return ESP_OK;
}

esp_err_t aht21_read(uint8_t i2c_bus, float *temperature, float *humidity)
{
    esp_err_t ret = aht21_trigger(i2c_bus);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(AHT21_MEASURE_DELAY_MS));
    return aht21_read_result(i2c_bus, temperature, humidity);
}

esp_err_t aht21_trigger(uint8_t i2c_bus)
{
    uint8_t trigger[] = {AHT21_CMD_MEASURE, AHT21_MEASURE_BYTE1, AHT21_MEASURE_BYTE2};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT21_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, trigger[0], true);
    i2c_master_write_byte(cmd, trigger[1], true);
    i2c_master_write_byte(cmd, trigger[2], true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t aht21_read_result(uint8_t i2c_bus, float *temperature, float *humidity)
{
    // Read 6 bytes: [status, hum_h, hum_l, hum_low4+temp_high4, temp_l, temp_low4]
    uint8_t data[6] = {0};

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT21_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_bus, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %d", ret);
        return ret;
    }

    // Check if measurement is valid (bit 7 = 0 means valid)
    if (data[0] & 0x80) {
        ESP_LOGW(TAG, "Measurement busy (status=0x%02X)", data[0]);
        return ESP_ERR_INVALID_STATE;
    }

    // Parse humidity (20 bits): data[1]<<12 | data[2]<<4 | data[3]>>4
    uint32_t raw_h = ((uint32_t)data[1] << 12) 
                   | ((uint32_t)data[2] << 4) 
                   | (data[3] >> 4);
    *humidity = ((float)raw_h / 1048576.0f) * 100.0f;  // / 2^20 * 100%

    // Parse temperature (20 bits): (data[3]&0x0F)<<16 | data[4]<<8 | data[5]
    uint32_t raw_t = ((uint32_t)(data[3] & 0x0F) << 16) 
                   | ((uint32_t)data[4] << 8) 
                   | data[5];
    *temperature = ((float)raw_t / 1048576.0f) * 200.0f - 50.0f;  // / 2^20 * 200 - 50

    return ESP_OK;
}
