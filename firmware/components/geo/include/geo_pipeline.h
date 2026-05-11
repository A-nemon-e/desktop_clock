#pragma once
#include "esp_err.h"
#include "types.h"  // geo_info_t defined in shared component

/**
 * @brief Execute 3-step IP geolocation pipeline:
 *    Step 1: Get public IP (api.ip.sb → ipify.org backup)
 *    Step 2: Get region/city/district/lat/lon (ipvx.netart.cn → ip-api.com backup)
 *    Step 3: Get IANA timezone (timezonedb.com → worldtimeapi.org → Asia/Shanghai fallback)
 *
 * @param info  [out] Populated geolocation info (zeroed first)
 * @return ESP_OK on success, ESP_FAIL if any step fails irrecoverably
 */
esp_err_t geo_pipeline_execute(geo_info_t *info);

/**
 * @brief Full WiFi→IP→Timezone→NTP auto-sync orchestrator.
 *        Calls geo_pipeline_execute(), applies TZ env var, syncs NTP.
 *        Runs in its own task — never blocks display_task.
 *
 * @return ESP_OK on complete success, ESP_FAIL otherwise
 */
esp_err_t geo_auto_sync(void);
