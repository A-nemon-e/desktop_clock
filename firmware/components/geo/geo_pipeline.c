#include "geo_pipeline.h"
#include "http_client.h"
#include "ntp_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define TAG "GEO"
#define TIMEZONEDB_API_KEY "YOUR_KEY"

static esp_err_t get_public_ip(char *ip, size_t len)
{
    cJSON *json = NULL;
    esp_err_t ret = http_get_with_backup(
        "https://api.ip.sb/geoip",
        "https://api.ipify.org?format=json",
        &json, 10000);
    if (ret != ESP_OK) return ret;
    cJSON *ip_obj = cJSON_GetObjectItem(json, "ip");
    if (ip_obj) strncpy(ip, ip_obj->valuestring, len);
    else { cJSON_Delete(json); return ESP_FAIL; }
    cJSON_Delete(json);
    return ESP_OK;
}

static esp_err_t get_region(const char *ip, geo_info_t *info)
{
    char url[256];
    cJSON *json = NULL;

    snprintf(url, sizeof(url), "https://ipvx.netart.cn/?ip=%s", ip);
    if (http_get_json(url, &json, 10000) == ESP_OK) {
        cJSON *regions = cJSON_GetObjectItem(json, "regions");
        if (regions && cJSON_GetArraySize(regions) >= 3) {
            strncpy(info->region, cJSON_GetArrayItem(regions, 0)->valuestring, 64);
            strncpy(info->city, cJSON_GetArrayItem(regions, 1)->valuestring, 64);
            strncpy(info->district, cJSON_GetArrayItem(regions, 2)->valuestring, 64);
            cJSON_Delete(json);
            return ESP_OK;
        }
        cJSON_Delete(json);
    }

    snprintf(url, sizeof(url),
             "http://ip-api.com/json/%s?fields=country,regionName,city,lat,lon", ip);
    if (http_get_json(url, &json, 10000) == ESP_OK) {
        cJSON *r = cJSON_GetObjectItem(json, "regionName");
        if (r) strncpy(info->region, r->valuestring, 64);
        r = cJSON_GetObjectItem(json, "city");
        if (r) strncpy(info->city, r->valuestring, 64);
        r = cJSON_GetObjectItem(json, "country");
        if (r) strncpy(info->country, r->valuestring, 64);
        r = cJSON_GetObjectItem(json, "lat");
        if (r) info->latitude = (float)r->valuedouble;
        r = cJSON_GetObjectItem(json, "lon");
        if (r) info->longitude = (float)r->valuedouble;
        cJSON_Delete(json);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t get_timezone(geo_info_t *info)
{
    char url[256];
    cJSON *json = NULL;

    snprintf(url, sizeof(url),
             "http://api.timezonedb.com/v2.1/get-time-zone"
             "?key=%s&format=json&by=position&lat=%.4f&lng=%.4f",
             TIMEZONEDB_API_KEY, info->latitude, info->longitude);
    if (http_get_json(url, &json, 10000) == ESP_OK) {
        cJSON *tz = cJSON_GetObjectItem(json, "zoneName");
        if (tz) {
            strncpy(info->timezone, tz->valuestring, 64);
            cJSON_Delete(json);
            return ESP_OK;
        }
        cJSON_Delete(json);
    }

    snprintf(url, sizeof(url),
             "http://worldtimeapi.org/api/timezone/Asia/%s",
             info->city[0] ? info->city : "Shanghai");
    if (http_get_json(url, &json, 10000) == ESP_OK) {
        cJSON *tz = cJSON_GetObjectItem(json, "timezone");
        if (tz) {
            strncpy(info->timezone, tz->valuestring, 64);
            cJSON_Delete(json);
            return ESP_OK;
        }
        cJSON_Delete(json);
    }

    strncpy(info->timezone, "Asia/Shanghai", 64);
    return ESP_OK;
}

esp_err_t geo_pipeline_execute(geo_info_t *info)
{
    memset(info, 0, sizeof(*info));

    ESP_LOGI(TAG, "Step 1/3: Getting public IP...");
    if (get_public_ip(info->public_ip, sizeof(info->public_ip)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get public IP");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Step 2/3: Getting region for %s...", info->public_ip);
    if (get_region(info->public_ip, info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get region");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Step 3/3: Getting timezone...");
    if (get_timezone(info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get timezone");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Pipeline complete: IP=%s, TZ=%s, %s %s %s",
             info->public_ip, info->timezone,
             info->region, info->city, info->district);
    return ESP_OK;
}

esp_err_t geo_auto_sync(void)
{
    geo_info_t info;

    ESP_LOGI(TAG, "Starting geo auto-sync...");
    if (geo_pipeline_execute(&info) != ESP_OK) {
        ESP_LOGE(TAG, "Geo pipeline failed");
        return ESP_FAIL;
    }

    setenv("TZ", info.timezone, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to %s, syncing NTP...", info.timezone);

    if (ntp_sync() != ESP_OK) {
        ESP_LOGW(TAG, "NTP sync failed (timezone already applied)");
    }

    return ESP_OK;
}
