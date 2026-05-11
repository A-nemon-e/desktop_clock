#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#define TAG "HTTP"
#define MAX_RESP_SIZE 4096

esp_err_t http_get(const char *url, http_response_t *resp, int timeout_ms)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = timeout_ms,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t ret = esp_http_client_perform(client);
    resp->status_code = esp_http_client_get_status_code(client);
    if (ret == ESP_OK && resp->status_code == 200) {
        int len = esp_http_client_get_content_length(client);
        if (len <= 0 || len > MAX_RESP_SIZE) len = MAX_RESP_SIZE;
        resp->data = malloc(len + 1);
        resp->len = esp_http_client_read(client, resp->data, len);
        resp->data[resp->len] = 0;
    }
    esp_http_client_cleanup(client);
    return (ret == ESP_OK && resp->status_code == 200) ? ESP_OK : ESP_FAIL;
}

esp_err_t http_get_json(const char *url, cJSON **json_out, int timeout_ms)
{
    http_response_t resp = {0};
    if (http_get(url, &resp, timeout_ms) != ESP_OK) return ESP_FAIL;
    *json_out = cJSON_Parse(resp.data);
    http_response_free(&resp);
    return (*json_out) ? ESP_OK : ESP_FAIL;
}

esp_err_t http_get_with_backup(const char *primary, const char *backup, cJSON **json_out, int timeout_ms)
{
    if (http_get_json(primary, json_out, timeout_ms) == ESP_OK) return ESP_OK;
    ESP_LOGW(TAG, "Primary failed, trying backup");
    return http_get_json(backup, json_out, timeout_ms);
}

void http_response_free(http_response_t *resp)
{
    if (resp->data) free(resp->data);
}
