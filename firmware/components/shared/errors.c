#include "errors.h"

const char* error_to_string(error_code_t code)
{
    switch (code) {
        case ERR_OK: return "OK";
        case ERR_WIFI_NOT_INIT: return "WiFi not initialized";
        case ERR_WIFI_TIMEOUT: return "WiFi connection timeout";
        case ERR_WIFI_AUTH_FAIL: return "WiFi authentication failed";
        case ERR_WIFI_DISCONNECTED: return "WiFi disconnected";
        case ERR_WIFI_AP_START_FAIL: return "WiFi AP mode start failed";
        case ERR_HTTP_TIMEOUT: return "HTTP request timeout";
        case ERR_HTTP_CONNECT_FAIL: return "HTTP connection failed";
        case ERR_HTTP_BAD_RESPONSE: return "HTTP bad response";
        case ERR_HTTP_ALL_BACKUPS_FAILED: return "All backup APIs failed";
        case ERR_MQTT_NOT_CONNECTED: return "MQTT not connected";
        case ERR_MQTT_PUBLISH_FAIL: return "MQTT publish failed";
        case ERR_MQTT_BACKEND_UNREACHABLE: return "Backend unreachable";
        case ERR_OTA_DOWNLOAD_FAIL: return "OTA download failed";
        case ERR_OTA_VERIFY_FAIL: return "OTA verification failed";
        case ERR_OTA_PARTITION_FAIL: return "OTA partition switch failed";
        case ERR_OTA_ROLLBACK: return "OTA rolled back";
        case ERR_OTA_IN_PROGRESS: return "OTA already in progress";
        case ERR_I2C_INIT_FAIL: return "I2C initialization failed";
        case ERR_I2C_WRITE_FAIL: return "I2C write failed";
        case ERR_I2C_READ_FAIL: return "I2C read failed";
        case ERR_I2C_TIMEOUT: return "I2C timeout";
        case ERR_SENSOR_READ_FAIL: return "Sensor read failed";
        case ERR_CONFIG_NVS_FAIL: return "NVS config failed";
        case ERR_CONFIG_INVALID: return "Invalid configuration";
        case ERR_CONFIG_SYNC_FAIL: return "Config sync failed";
        case ERR_NOT_IMPLEMENTED: return "Not implemented";
        default: return "Unknown error";
    }
}
