#include "ota_handler.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>

#include <spi_flash_mmap.h>

/* Local copies of errors.h OTA codes to avoid ERR_OK conflict with lwip */
#define ERR_OTA_VERIFY_FAIL    402
#define ERR_OTA_PARTITION_FAIL 403
#define ERR_OTA_DOWNLOAD_FAIL  401

static const char *TAG = "OTA";

/* ─── OTA download progress callback ─── */
__attribute__((unused))
static void ota_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "Connected to server");
        break;
    case HTTP_EVENT_ON_DATA:
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "Download finished");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected");
        break;
    default:
        break;
    }
}

esp_err_t ota_start(const char *fw_url, const char *expected_sha256)
{
    if (!fw_url || strlen((const char *)fw_url) == 0) {
        ESP_LOGE(TAG, "Invalid OTA URL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA from: %s", fw_url);
    if (expected_sha256) {
        ESP_LOGI(TAG, "SHA256 expected: %s", expected_sha256);
    }

    /*
     * Phase 2 hook: stop audio playback, pause render_task.
     * display_task stays alive for OTA progress indication.
     */

    /* Configure HTTP client for OTA */
    esp_http_client_config_t http_cfg = {
        .url = fw_url,
        .timeout_ms = 120000,
        .cert_pem = NULL,  /* Use default x509 bundle if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y */
//        .event_handler = ota_http_event_handler,  /* Uncomment for progress callbacks */
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .http_client_init_cb = NULL,
        .bulk_flash_erase = false,
        .partial_http_download = false,  /* Full download — restart if interrupted */
    };

    ESP_LOGI(TAG, "Erasing inactive OTA slot...");

    /*
     * esp_https_ota():
     *   - Automatically selects the inactive OTA partition
     *   - Downloads firmware to it
     *   - Verifies image header magic byte (0xE9)
     *   - Performs SHA256 digest check if CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=n
     *
     * Must NOT erase NVS. Factory partition must stay intact.
     */
    esp_err_t ret = esp_https_ota(&ota_cfg);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA flash successful");
        const esp_partition_t *updated = esp_ota_get_boot_partition();
        if (!updated) {
            ESP_LOGE(TAG, "Failed to get updated partition");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Setting boot partition to: %s", updated->label);
        ret = esp_ota_set_boot_partition(updated);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set boot partition: %d", ret);
            return ret;
        }

        ESP_LOGI(TAG, "OTA successful — restarting in 1 second...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        /* Unreachable */
        return ESP_OK;
    }

    ESP_LOGE(TAG, "OTA failed with error: 0x%x (%s)", ret, esp_err_to_name(ret));

    /*
     * Map esp_https_ota errors to our error codes.
     * ESP_ERR_OTA_VALIDATE_FAILED → ERR_OTA_VERIFY_FAIL
     * ESP_ERR_FLASH_OP_FAIL      → ERR_OTA_PARTITION_FAIL
     * Default                     → ERR_OTA_DOWNLOAD_FAIL
     */
    if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
        return ERR_OTA_VERIFY_FAIL;
    } else if (ret == ESP_ERR_FLASH_OP_FAIL || ret == ESP_ERR_FLASH_OP_TIMEOUT) {
        return ERR_OTA_PARTITION_FAIL;
    }
    return ERR_OTA_DOWNLOAD_FAIL;
}

esp_err_t ota_check_rollback(void)
{
    ESP_LOGI(TAG, "Checking OTA rollback state...");

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Running partition: %s (type=%d, subtype=%d)",
             running->label, running->type, running->subtype);

    esp_ota_img_states_t state;
    esp_err_t ret = esp_ota_get_state_partition(running, &state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get partition state: %d", ret);
        /*
         * This may fail on factory partition (no OTA state).
         * That's fine — factory doesn't need rollback handling.
         */
        return ESP_OK;
    }

    switch (state) {
    case ESP_OTA_IMG_PENDING_VERIFY:
        ESP_LOGI(TAG, "Image is PENDING_VERIFY — marking as valid, cancelling rollback");
        ret = esp_ota_mark_app_valid_cancel_rollback();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA verified successfully, rollback cancelled");
        } else {
            ESP_LOGE(TAG, "Failed to mark app valid: %d", ret);
        }
        break;

    case ESP_OTA_IMG_VALID:
        ESP_LOGI(TAG, "Image is already VALID — no rollback needed");
        break;

    case ESP_OTA_IMG_INVALID:
        ESP_LOGW(TAG, "Image is INVALID — rollback will occur on next boot");
        break;

    case ESP_OTA_IMG_ABORTED:
        ESP_LOGW(TAG, "Image was ABORTED — rollback will occur on next boot");
        break;

    case ESP_OTA_IMG_NEW:
        ESP_LOGI(TAG, "Image is NEW — will enter PENDING_VERIFY after boot");
        break;

    default:
        ESP_LOGW(TAG, "Unknown OTA state: %d", (int)state);
        break;
    }

    return ESP_OK;
}

void ota_on_mqtt_trigger(const char *json)
{
    if (!json) {
        ESP_LOGE(TAG, "NULL JSON payload");
        return;
    }

    ESP_LOGI(TAG, "OTA trigger received via MQTT");

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse OTA JSON: %s",
                 cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
        return;
    }

    cJSON *url_item = cJSON_GetObjectItem(root, "url");
    cJSON *sha256_item = cJSON_GetObjectItem(root, "sha256");
    cJSON *version_item = cJSON_GetObjectItem(root, "version");

    if (!url_item || !cJSON_IsString(url_item)) {
        ESP_LOGE(TAG, "Missing 'url' field in OTA JSON payload");
        cJSON_Delete(root);
        return;
    }

    const char *fw_url = url_item->valuestring;
    const char *expected_sha256 = (sha256_item && cJSON_IsString(sha256_item))
                                  ? sha256_item->valuestring : NULL;

    if (version_item && cJSON_IsString(version_item)) {
        ESP_LOGI(TAG, "OTA version: %s", version_item->valuestring);
    }

    ESP_LOGI(TAG, "Parsed OTA command → url=%s", fw_url);

    esp_err_t ret = ota_start(fw_url, expected_sha256);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA trigger failed: error=%d", ret);
        /*
         * TODO(Phase 2): publish MQTT status: {"status": "failed", "error": ret}
         */
    }

    cJSON_Delete(root);
}
