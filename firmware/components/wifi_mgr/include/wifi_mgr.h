#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * WiFi manager state machine
 */
typedef enum {
    WIFI_STATE_UNINIT = 0,   /**< Not yet initialized */
    WIFI_STATE_CONNECTING,   /**< Attempting STA connection */
    WIFI_STATE_CONNECTED,    /**< STA connected, got IP */
    WIFI_STATE_FAILED,       /**< STA connection failed (max retries) */
    WIFI_STATE_AP_MODE,      /**< Running in AP mode */
} wifi_state_t;

/**
 * Event group bits for synchronization.
 * Other tasks can wait on wifi_event_group for these bits.
 */
#define WIFI_CONNECTED_BIT  BIT0    /**< STA connected + got IP */
#define WIFI_AP_ACTIVE_BIT  BIT1    /**< AP mode is active */
#define WIFI_FAILED_BIT     BIT2    /**< STA connection failed after max retries */

/** Event group handle — created in wifi_mgr_init(), never destroyed */
extern EventGroupHandle_t wifi_event_group;

/**
 * Initialize WiFi subsystem.
 *
 * Reads saved credentials from NVS. If found, attempts STA connection.
 * If no credentials exist, starts AP mode for configuration.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_mgr_init(void);

/**
 * Get current WiFi state.
 *
 * @return Current wifi_state_t value
 */
wifi_state_t wifi_mgr_get_state(void);

/**
 * Save WiFi credentials to NVS and reboot.
 *
 * After saving, calls esp_restart() — this function never returns
 * on success.
 *
 * @param ssid      WiFi SSID (max 32 bytes, null-terminated)
 * @param password  WiFi password (max 64 bytes, null-terminated, empty for open)
 * @return ESP_OK on success (but reboots, so caller won't see this),
 *         or ESP_ERR_NVS_* on storage failure
 */
esp_err_t wifi_mgr_save_creds(const char *ssid, const char *password);

/**
 * Start AP mode for initial configuration.
 *
 * Generates SSID "Clock-Config-XXXX" where XXXX is the last 4 hex
 * digits of the MAC address. Open network, max 1 client.
 */
void wifi_mgr_start_ap(void);
