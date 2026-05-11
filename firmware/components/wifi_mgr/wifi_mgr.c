#include "wifi_mgr.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"

#include <string.h>

#define TAG "WIFI"

/** Maximum STA connection attempts before falling back to AP mode */
#define WIFI_MAX_RETRIES        3

/** Timeout for STA connection (seconds) */
#define WIFI_CONNECT_TIMEOUT_S  30

/** AP SSID prefix — suffix is last 4 hex digits of MAC for uniqueness */
#define WIFI_AP_SSID_PREFIX     "Clock-Config-"

/** NVS namespace for WiFi credentials */
#define WIFI_NVS_NAMESPACE      "wifi_config"

/* ---- Static state ---- */
static wifi_state_t state = WIFI_STATE_UNINIT;
EventGroupHandle_t wifi_event_group = NULL;
static int retry_count = 0;

/* ---- Forward declarations ---- */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data);

/* ================================================================
 * Event Handler (delegates to event group — no blocking)
 * ================================================================ */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            state = WIFI_STATE_CONNECTING;
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *ev =
                (wifi_event_sta_disconnected_t *)data;
            if (retry_count < WIFI_MAX_RETRIES) {
                retry_count++;
                ESP_LOGW(TAG, "Disconnected (reason=%d), retry %d/%d",
                         ev->reason, retry_count, WIFI_MAX_RETRIES);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "Max retries (%d) reached, switching to AP",
                         WIFI_MAX_RETRIES);
                state = WIFI_STATE_FAILED;
                xEventGroupSetBits(wifi_event_group, WIFI_FAILED_BIT);
            }
            break;
        }

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP started");
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *ev =
                (wifi_event_ap_staconnected_t *)data;
            ESP_LOGI(TAG, "Client connected: " MACSTR,
                     MAC2STR(ev->mac));
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *ev =
                (wifi_event_ap_stadisconnected_t *)data;
            ESP_LOGI(TAG, "Client disconnected: " MACSTR,
                     MAC2STR(ev->mac));
            break;
        }

        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        retry_count = 0;
        state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

esp_err_t wifi_mgr_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");

    /* Create event group */
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* Initialize TCP/IP stack + default STA/AP interfaces */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    /* Initialize WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* Check NVS for saved credentials */
    nvs_handle_t nvs;
    char ssid[33] = {0};
    char password[65] = {0};
    bool has_creds = false;

    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len = sizeof(ssid);
        if (nvs_get_str(nvs, "ssid", ssid, &len) == ESP_OK) {
            len = sizeof(password);
            if (nvs_get_str(nvs, "password", password, &len) == ESP_OK) {
                has_creds = true;
            }
        }
        nvs_close(nvs);
    }

    if (has_creds) {
        ESP_LOGI(TAG, "Found saved credentials for \"%s\", "
                 "attempting STA connection", ssid);
        state = WIFI_STATE_CONNECTING;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

        wifi_config_t sta_cfg = {0};
        strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
        strncpy((char *)sta_cfg.sta.password, password,
                sizeof(sta_cfg.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        ESP_LOGI(TAG, "No credentials found in NVS, starting AP mode");
        wifi_mgr_start_ap();
    }

    return ESP_OK;
}

void wifi_mgr_start_ap(void)
{
    state = WIFI_STATE_AP_MODE;

    /* Generate unique SSID using last 4 hex digits of MAC address */
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    uint16_t mac_suffix = ((uint16_t)mac[4] << 8) | mac[5];

    wifi_config_t ap_cfg = {0};
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid),
             "%s%04X", WIFI_AP_SSID_PREFIX, mac_suffix);
    ap_cfg.ap.ssid_len = strlen((char *)ap_cfg.ap.ssid);
    ap_cfg.ap.max_connection = 1;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupSetBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);
    ESP_LOGI(TAG, "AP \"%s\" started (open, max 1 client)", ap_cfg.ap.ssid);
}

wifi_state_t wifi_mgr_get_state(void)
{
    return state;
}

esp_err_t wifi_mgr_save_creds(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    err = nvs_set_str(nvs, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write password: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    nvs_close(nvs);
    ESP_LOGI(TAG, "Credentials saved for \"%s\", restarting...", ssid);

    /* Small delay to let UART flush before reboot */
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}
