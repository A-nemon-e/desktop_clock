#include "ntp_client.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "NTP"

static bool synced = false;

static void ntp_callback(struct timeval *tv)
{
    synced = true;
    ESP_LOGI(TAG, "Time synced");
}

esp_err_t ntp_sync(void)
{
    synced = false;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER_PRIMARY);
    esp_sntp_setservername(1, NTP_SERVER_BACKUP);
    esp_sntp_set_time_sync_notification_cb(ntp_callback);
    esp_sntp_init();

    int wait = 0;
    while (!synced && wait < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait++;
    }
    return synced ? ESP_OK : ESP_FAIL;
}
