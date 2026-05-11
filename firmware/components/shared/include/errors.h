#pragma once

/**
 * Unified Error Codes for Desktop Clock Firmware
 * 
 * Convention:
 *   - 0 = OK (success)
 *   - 1xx = WiFi errors
 *   - 2xx = HTTP/API errors  
 *   - 3xx = MQTT errors
 *   - 4xx = OTA errors
 *   - 5xx = Hardware/I2C errors
 *   - 6xx = Configuration errors
 */

typedef enum {
    // Success
    ERR_OK = 0,

    // WiFi errors (1xx)
    ERR_WIFI_NOT_INIT = 101,
    ERR_WIFI_TIMEOUT = 102,
    ERR_WIFI_AUTH_FAIL = 103,
    ERR_WIFI_DISCONNECTED = 104,
    ERR_WIFI_AP_START_FAIL = 105,

    // HTTP/API errors (2xx)
    ERR_HTTP_TIMEOUT = 201,
    ERR_HTTP_CONNECT_FAIL = 202,
    ERR_HTTP_BAD_RESPONSE = 203,
    ERR_HTTP_ALL_BACKUPS_FAILED = 204,

    // MQTT errors (3xx)
    ERR_MQTT_NOT_CONNECTED = 301,
    ERR_MQTT_PUBLISH_FAIL = 302,
    ERR_MQTT_BACKEND_UNREACHABLE = 303,

    // OTA errors (4xx)
    ERR_OTA_DOWNLOAD_FAIL = 401,
    ERR_OTA_VERIFY_FAIL = 402,
    ERR_OTA_PARTITION_FAIL = 403,
    ERR_OTA_ROLLBACK = 404,
    ERR_OTA_IN_PROGRESS = 405,

    // Hardware/I2C errors (5xx)
    ERR_I2C_INIT_FAIL = 501,
    ERR_I2C_WRITE_FAIL = 502,
    ERR_I2C_READ_FAIL = 503,
    ERR_I2C_TIMEOUT = 504,
    ERR_SENSOR_READ_FAIL = 505,

    // Configuration errors (6xx)
    ERR_CONFIG_NVS_FAIL = 601,
    ERR_CONFIG_INVALID = 602,
    ERR_CONFIG_SYNC_FAIL = 603,

    // General
    ERR_UNKNOWN = 999,
    ERR_NOT_IMPLEMENTED = 998,
} error_code_t;

// Convert error code to human-readable string
const char* error_to_string(error_code_t code);
