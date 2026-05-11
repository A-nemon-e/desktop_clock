#include "config_sync.h"
#include "clock_mqtt.h"
#include "card_mgr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define TAG "CFG"
#define NVS_NAMESPACE "config"
#define NVS_KEY_JSON  "json"
#define CONFIG_TOPIC_PREFIX "clock/"

static uint32_t s_local_version = 0;
static char s_mac[18] = {0};
static bool s_initialized = false;

static void on_mqtt_config_msg(const char *topic, const char *data, void *arg)
{
    (void)arg;

    char expected_topic[128];
    snprintf(expected_topic, sizeof(expected_topic),
             "clock/%s/config", s_mac);

    if (strcmp(topic, expected_topic) != 0) return;

    ESP_LOGI(TAG, "Received config push via MQTT");
    config_sync_on_mqtt_message(data);
}

esp_err_t config_sync_init(const char *mac)
{
    if (mac) {
        strncpy(s_mac, mac, sizeof(s_mac) - 1);
        s_mac[sizeof(s_mac) - 1] = '\0';
    }

    char *buf = NULL;
    size_t len = 0;
    esp_err_t err = config_sync_load_from_nvs(&buf, &len);

    if (err == ESP_OK && buf) {
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *v = cJSON_GetObjectItem(root, "v");
            if (v) s_local_version = (uint32_t)v->valueint;
            cJSON_Delete(root);
        }
        free(buf);
    }

    mqtt_register_msg_callback(on_mqtt_config_msg, NULL);
    s_initialized = true;

    ESP_LOGI(TAG, "Config sync initialized, local version=%"PRIu32, s_local_version);
    return ESP_OK;
}

esp_err_t config_sync_on_mqtt_message(const char *json)
{
    if (!json) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "Invalid config JSON from MQTT");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *rver = cJSON_GetObjectItem(root, "v");
    if (!rver) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "Config JSON missing version field");
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t remote_version = (uint32_t)rver->valueint;

    if (remote_version > s_local_version) {
        ESP_LOGI(TAG, "Remote config v%"PRIu32" > local v%"PRIu32" — applying",
                 remote_version, s_local_version);

        s_local_version = remote_version;
        config_sync_save_to_nvs(json);

        cJSON *cards = cJSON_GetObjectItem(root, "cards");
        if (cards && cJSON_IsArray(cards)) {
            card_mgr_load_from_nvs();
            ESP_LOGI(TAG, "Cards reloaded from synced config");
        }
    } else if (remote_version < s_local_version) {
        ESP_LOGI(TAG, "Remote config v%"PRIu32" < local v%"PRIu32" — reporting local",
                 remote_version, s_local_version);
        config_sync_report_to_server();
    } else {
        ESP_LOGD(TAG, "Config version match (v%"PRIu32") — ignoring", remote_version);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t config_sync_report_to_server(void)
{
    char *buf = NULL;
    size_t len = 0;

    esp_err_t err = config_sync_load_from_nvs(&buf, &len);
    if (err != ESP_OK || !buf) {
        ESP_LOGW(TAG, "No local config to report");
        return ESP_ERR_NOT_FOUND;
    }

    char topic[128];
    snprintf(topic, sizeof(topic), "clock/%s/config_report", s_mac);
    mqtt_publish(topic, buf);
    free(buf);

    ESP_LOGI(TAG, "Config reported to server (v%"PRIu32")", s_local_version);
    return ESP_OK;
}

uint32_t config_sync_get_local_version(void)
{
    return s_local_version;
}

void config_sync_increment_version(void)
{
    s_local_version++;

    char *buf = NULL;
    size_t len = 0;
    if (config_sync_load_from_nvs(&buf, &len) == ESP_OK && buf) {
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON_DeleteItemFromObject(root, "v");
            cJSON_AddNumberToObject(root, "v", (double)s_local_version);
            char *updated = cJSON_PrintUnformatted(root);
            if (updated) {
                config_sync_save_to_nvs(updated);
                free(updated);
            }
            cJSON_Delete(root);
        }
        free(buf);
    }
}

esp_err_t config_sync_save_to_nvs(const char *json)
{
    nvs_handle_t n;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &n);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(n, NVS_KEY_JSON, json);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set_str failed: %s", esp_err_to_name(err));
        nvs_close(n);
        return err;
    }

    err = nvs_commit(n);
    nvs_close(n);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to NVS (%zu bytes)", strlen(json));
    }
    return err;
}

esp_err_t config_sync_load_from_nvs(char **buf, size_t *len)
{
    nvs_handle_t n;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &n);
    if (err != ESP_OK) return err;

    size_t required = 0;
    err = nvs_get_str(n, NVS_KEY_JSON, NULL, &required);
    if (err != ESP_OK || required == 0) {
        nvs_close(n);
        return ESP_ERR_NOT_FOUND;
    }

    *buf = malloc(required);
    if (!*buf) {
        nvs_close(n);
        return ESP_ERR_NO_MEM;
    }

    err = nvs_get_str(n, NVS_KEY_JSON, *buf, &required);
    nvs_close(n);

    if (err != ESP_OK) {
        free(*buf);
        *buf = NULL;
        return err;
    }

    if (len) *len = required;
    return ESP_OK;
}


