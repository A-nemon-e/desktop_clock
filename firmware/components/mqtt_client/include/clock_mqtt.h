#pragma once
#include "esp_err.h"
#include <stdbool.h>

/**
 * @file mqtt_client.h
 * @brief MQTT client with LWT, exponential backoff, and fat/thin mode switching.
 *
 * Topic convention:
 *   clock/{mac}/status  — online/offline status + device info (publish)
 *   clock/{mac}/config  — card configuration JSON (subscribe)
 *   clock/{mac}/ota     — OTA firmware URL + version (subscribe)
 *   clock/{mac}/weather — weather data push (subscribe)
 *
 * LWT: clock/{mac}/status = {"online":false} retained
 *
 * Fat/Thin mode:
 *   3 consecutive connection failures → switch to thin mode
 *   Thin mode retries every 120s → success → exit thin mode
 *
 * Reconnection: exponential backoff 1s→2s→4s→8s→…→max 60s
 */

/**
 * @brief Callback type for incoming MQTT messages.
 *
 * @param topic  Full topic string (null-terminated)
 * @param data   Payload (null-terminated JSON)
 * @param arg    User context pointer
 */
typedef void (*mqtt_msg_cb_t)(const char *topic, const char *data, void *arg);

/**
 * @brief Initialize MQTT client, set LWT, register event handler, and start.
 *
 * Configures client_id as MAC address (must be set via mqtt_set_mac() first).
 * LWT message: clock/{mac}/status = "offline", will retained.
 *
 * On MQTT_EVENT_CONNECTED:
 *   - Publishes online status
 *   - Subscribes to clock/{mac}/config, clock/{mac}/ota, clock/{mac}/weather
 *   - Resets fail_count, exits thin mode if active
 *
 * On MQTT_EVENT_DISCONNECTED:
 *   - Increments fail_count
 *   - Starts reconnect timer with exponential backoff
 *   - After 3 consecutive failures, enters thin mode
 *
 * On MQTT_EVENT_DATA:
 *   - Routes to registered message callbacks
 *
 * @param broker_url  MQTT broker URI (e.g., "mqtt://192.168.1.100:1883")
 * @return ESP_OK on success, ESP_FAIL on init failure
 */
esp_err_t mqtt_client_init(const char *broker_url);

/**
 * @brief Set device MAC address (used for topic construction).
 *
 * Must be called before mqtt_client_init(). Typically reads from
 * esp_efuse_mac or WiFi MAC.
 *
 * @param mac  MAC address string (e.g., "aa:bb:cc:dd:ee:ff")
 */
void mqtt_set_mac(const char *mac);

/**
 * @brief Publish device online status with full device info.
 *
 * Topic: clock/{mac}/status
 * Payload: {"online":true,"version":"1.0.0","hw":"0.1","ip":"x.x.x.x","region":"..."}
 *
 * @param version  Firmware version string
 * @param hw_rev   Hardware revision string
 * @param ip       Device IP address
 * @param region   Geo-located region string
 * @return ESP_OK on success
 */
esp_err_t mqtt_publish_status(const char *version, const char *hw_rev,
                               const char *ip, const char *region);

/**
 * @brief Publish a JSON payload to a custom topic.
 *
 * @param topic  Full topic string
 * @param json   JSON payload (null-terminated)
 * @return ESP_OK on success
 */
esp_err_t mqtt_publish(const char *topic, const char *json);

/**
 * @brief Check if MQTT is currently connected.
 *
 * @return true if connected, false otherwise
 */
bool mqtt_is_connected(void);

/**
 * @brief Check if device is in thin (offline) mode.
 *
 * @return true if thin mode is active
 */
bool mqtt_is_thin_mode(void);

/**
 * @brief Register a callback for incoming MQTT messages.
 *
 * Multiple callbacks can be registered. Each is called for every
 * incoming message. The callback should check the topic and
 * return quickly (no blocking operations).
 *
 * @param cb   Callback function
 * @param arg  User context passed to callback
 */
void mqtt_register_msg_callback(mqtt_msg_cb_t cb, void *arg);

/**
 * @brief Stop and clean up MQTT client.
 *
 * Disconnects gracefully (publishes LWT "offline" before closing).
 */
void mqtt_client_deinit(void);
