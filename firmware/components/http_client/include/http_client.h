#pragma once
#include "esp_err.h"
#include "cJSON.h"

typedef struct {
    char *data;
    int len;
    int status_code;
} http_response_t;

esp_err_t http_get(const char *url, http_response_t *resp, int timeout_ms);
esp_err_t http_get_json(const char *url, cJSON **json_out, int timeout_ms);
esp_err_t http_get_with_backup(const char *primary, const char *backup, cJSON **json_out, int timeout_ms);
void http_response_free(http_response_t *resp);
