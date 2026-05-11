#pragma once
#include "esp_err.h"

/**
 * OTA Handler — HTTPS firmware update with dual-partition rollback
 *
 * Uses ESP-IDF OTA API: esp_https_ota(), esp_ota_get_state_partition(),
 * esp_ota_mark_app_valid_cancel_rollback().
 *
 * Partition layout: factory (fallback), ota_0, ota_1 (two-slot OTA).
 * SHA256 verification performed before switching boot partition.
 * Crash recovery: bootloader auto-rollback to last valid partition.
 */

/** OTA URL maximum length */
#define OTA_URL_MAX_LEN 256

/**
 * Start OTA download + flash + verify + restart.
 *
 * Stops non-critical tasks (audio/render), keeps display_task alive
 * for progress indication. Downloads firmware to the inactive OTA slot,
 * verifies SHA256 if provided, sets boot partition, then restarts.
 *
 * @param fw_url          HTTPS firmware URL (max OTA_URL_MAX_LEN)
 * @param expected_sha256 Optional SHA256 hex string for verification (NULL = skip)
 * @return ESP_OK on successful download + flash (then esp_restart() called)
 *         ESP_FAIL / ERR_OTA_DOWNLOAD_FAIL / ERR_OTA_VERIFY_FAIL on error
 */
esp_err_t ota_start(const char *fw_url, const char *expected_sha256);

/**
 * Check and handle OTA rollback state (call once at boot after NVS init).
 *
 * If the running partition is PENDING_VERIFY and the app has started
 * successfully, marks it as valid and cancels the rollback.
 * If the app crashes before this is called, the bootloader will
 * automatically roll back to the previous valid partition.
 *
 * @return ESP_OK (always — failures are logged, not propagated)
 */
esp_err_t ota_check_rollback(void);

/**
 * Handle MQTT OTA trigger message.
 *
 * Expects JSON: {"url": "https://...", "version": "x.y.z", "sha256": "abc..."}
 * Parses and calls ota_start().
 *
 * @param json Null-terminated JSON string from MQTT payload
 */
void ota_on_mqtt_trigger(const char *json);
