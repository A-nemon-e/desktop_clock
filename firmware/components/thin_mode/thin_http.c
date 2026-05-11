#include "thin_http.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "THIN"

static httpd_handle_t server = NULL;

static const char *HTML = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Clock Setup</title><style>body{font:16px sans-serif;max-width:320px;margin:20px auto;padding:10px}"
"input,select,button{width:100%;padding:8px;margin:6px 0;box-sizing:border-box}"
"button{background:#1a73e8;color:#fff;border:none;border-radius:4px;font-size:16px}</style></head><body>"
"<h2>桌面时钟配置</h2><p>仅支持2.4GHz WiFi</p>"
"<form method='POST' action='/api/config'>"
"<label>WiFi SSID</label><input name='ssid' required>"
"<label>密码</label><input name='password' type='password'>"
"<label>后端地址</label><input name='backend_url' placeholder='http://192.168.x.x:8000'>"
"<label>备用后端</label><input name='backup_url'>"
"<button type='submit'>保存并重启</button></form></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML, strlen(HTML));
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (len <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }

    char ssid[33] = {0}, password[65] = {0}, backend[128] = {0}, backup[128] = {0};
    char *p = strtok(buf, "&");
    while (p) {
        if (strncmp(p, "ssid=", 5) == 0) {
            char *v = p+5; char *d = ssid;
            while (*v && *v != '&') { if (*v == '+') *d++ = ' '; else if (*v == '%') { int h; sscanf(v+1, "%2x", &h); *d++ = h; v+=2; } else *d++ = *v; v++; }
        }
        else if (strncmp(p, "password=", 9) == 0) {
            char *v = p+9; char *d = password;
            while (*v && *v != '&') { if (*v == '+') *d++ = ' '; else *d++ = *v; v++; }
        }
        p = strtok(NULL, "&");
    }

    nvs_handle_t nvs;
    nvs_open("wifi_config", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", password);
    nvs_commit(nvs);
    nvs_close(nvs);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, "<h2>已保存，正在重启...</h2>");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

esp_err_t thin_http_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) != ESP_OK) return ESP_FAIL;
    httpd_uri_t root = {.uri="/", .method=HTTP_GET, .handler=root_handler};
    httpd_register_uri_handler(server, &root);
    httpd_uri_t cfg = {.uri="/api/config", .method=HTTP_POST, .handler=config_handler};
    httpd_register_uri_handler(server, &cfg);
    ESP_LOGI(TAG, "Thin mode HTTP server started");
    return ESP_OK;
}

esp_err_t thin_http_stop(void)
{
    if (server) { httpd_stop(server); server = NULL; }
    return ESP_OK;
}
