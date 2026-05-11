#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file config_sync.h
 * @brief Configuration synchronization between NVS and MQTT backend.
 *
 * Protocol:
 *   - Incoming MQTT topic: clock/{mac}/config → JSON with version field
 *   - Outgoing report:     clock/{mac}/config_report → full config JSON
 *
 * Versioning:
 *   v=remote > v=local  → overwrite NVS, hot-apply (no reboot needed)
 *   v=remote < v=local  → report local to server (server should correct)
 *   v=remote == v=local → ignore
 *
 * JSON config structure:
 *   { "v":<int>, "cards":[...], "wifi":{...}, "backend":{...} }
 */

/**
 * @brief Initialize config sync subsystem.
 *
 * Registers as MQTT message callback for clock/{mac}/config topic.
 * Reads current local config version from NVS.
 *
 * @param mac  Device MAC address string (e.g., "aa:bb:cc:dd:ee:ff")
 * @return ESP_OK on success
 */
esp_err_t config_sync_init(const char *mac);

/**
 * @brief Process an incoming MQTT config message.
 *
 * Called by mqtt_client when a message arrives on clock/{mac}/config.
 * Parses JSON, compares version, applies if newer.
 *
 * @param json  Full JSON payload from MQTT message
 * @return ESP_OK on success (applied or ignored)
 */
esp_err_t config_sync_on_mqtt_message(const char *json);

/**
 * @brief Publish current local config to MQTT.
 *
 * Topic: clock/{mac}/config_report
 * Payload: full config JSON with current version
 *
 * Called automatically on MQTT connect, or manually to force sync.
 *
 * @return ESP_OK on success
 */
esp_err_t config_sync_report_to_server(void);

/**
 * @brief Get current local config version.
 *
 * @return Version number (1-based), or 0 if uninitialized
 */
uint32_t config_sync_get_local_version(void);

/**
 * @brief Increment local config version and save.
 *
 * Called after local changes (e.g., card added via button UI).
 */
void config_sync_increment_version(void);

/**
 * @brief Persist full config JSON to NVS.
 *
 * Used internally and by card_mgr for saving state.
 *
 * @param json  Full config JSON string
 * @return ESP_OK on success
 */
esp_err_t config_sync_save_to_nvs(const char *json);

/**
 * @brief Load full config JSON from NVS.
 *
 * @param buf   Output buffer (caller must free)
 * @param len   [in/out] buffer size on input, actual length on output
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no saved config
 */
esp_err_t config_sync_load_from_nvs(char **buf, size_t *len);
