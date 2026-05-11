#include "clock_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#define TAG "MQT"
#define MAX_CALLBACKS 4
#define THIN_RETRY_MS  120000
#define BACKOFF_CAP_MS 60000
#define BACKOFF_BASE_MS 1000

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
} mqtt_state_t;

static esp_mqtt_client_handle_t client = NULL;
static bool s_thin_mode = false;
static int s_fail_count = 0;
static uint32_t s_backoff_ms = BACKOFF_BASE_MS;
static mqtt_state_t s_state = MQTT_STATE_DISCONNECTED;
static char s_mac[18] = {0};
static char s_broker_url[256] = {0};
static char s_lwt_topic[64] = {0};

static mqtt_msg_cb_t s_callbacks[MAX_CALLBACKS];
static void *s_cb_args[MAX_CALLBACKS];
static int s_cb_count = 0;

static TimerHandle_t s_reconnect_timer = NULL;

static void start_reconnect_timer(void);
static void publish_online_status(void);
static void subscribe_topics(void);

static void dispatch_message(const char *topic, int topic_len,
                              const char *data, int data_len)
{
    char topic_buf[128];
    char data_buf[512];
    int tlen = topic_len < (int)sizeof(topic_buf)-1 ? topic_len : (int)sizeof(topic_buf)-1;
    int dlen = data_len < (int)sizeof(data_buf)-1 ? data_len : (int)sizeof(data_buf)-1;
    memcpy(topic_buf, topic, tlen);
    topic_buf[tlen] = '\0';
    memcpy(data_buf, data, dlen);
    data_buf[dlen] = '\0';

    for (int i = 0; i < s_cb_count; i++) {
        if (s_callbacks[i]) {
            s_callbacks[i](topic_buf, data_buf, s_cb_args[i]);
        }
    }
}

static void reconnect_timer_cb(TimerHandle_t timer)
{
    if (s_state == MQTT_STATE_CONNECTED) return;
    ESP_LOGI(TAG, "Reconnect attempt (fail=%d, backoff=%"PRIu32"ms, thin=%d)",
             s_fail_count, s_backoff_ms, s_thin_mode);

    if (client) {
        esp_mqtt_client_reconnect(client);
    }
    s_state = MQTT_STATE_CONNECTING;
}

static void start_reconnect_timer(void)
{
    if (s_reconnect_timer) {
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_backoff_ms), 0);
        xTimerStart(s_reconnect_timer, 0);
    }
}

static void schedule_reconnect(void)
{
    if (s_thin_mode) {
        s_backoff_ms = THIN_RETRY_MS;
    } else {
        if (s_fail_count >= 3) {
            s_thin_mode = true;
            s_backoff_ms = THIN_RETRY_MS;
            ESP_LOGW(TAG, "Backend unreachable after %d failures, entering thin mode", s_fail_count);
        }
    }
    s_state = MQTT_STATE_DISCONNECTED;
    start_reconnect_timer();
}

static void publish_online_status(void)
{
    if (!client || s_mac[0] == '\0') return;
    char topic[128], payload[256];
    snprintf(topic, sizeof(topic), "clock/%s/status", s_mac);
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"lwt\":false}");
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
}

static void publish_offline_status(void)
{
    if (!client || s_mac[0] == '\0') return;
    char topic[128];
    snprintf(topic, sizeof(topic), "clock/%s/status", s_mac);
    esp_mqtt_client_publish(client, topic, "{\"online\":false}", 0, 1, 1);
}

static void subscribe_topics(void)
{
    if (!client || s_mac[0] == '\0') return;
    char topic[128];
    snprintf(topic, sizeof(topic), "clock/%s/config", s_mac);
    esp_mqtt_client_subscribe(client, topic, 1);
    snprintf(topic, sizeof(topic), "clock/%s/ota", s_mac);
    esp_mqtt_client_subscribe(client, topic, 1);
    snprintf(topic, sizeof(topic), "clock/%s/weather", s_mac);
    esp_mqtt_client_subscribe(client, topic, 1);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_state = MQTT_STATE_CONNECTED;
        s_fail_count = 0;
        s_backoff_ms = BACKOFF_BASE_MS;
        if (s_thin_mode) {
            s_thin_mode = false;
            ESP_LOGI(TAG, "Reconnected — exiting thin mode");
        }
        ESP_LOGI(TAG, "Connected to broker");
        publish_online_status();
        subscribe_topics();
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_fail_count++;
        if (s_state == MQTT_STATE_CONNECTED || s_state == MQTT_STATE_CONNECTING) {
            ESP_LOGW(TAG, "Disconnected (fail %d)", s_fail_count);
        }
        schedule_reconnect();
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "RX topic=%.*s payload=%.*s",
                 ev->topic_len, ev->topic,
                 ev->data_len, ev->data);
        dispatch_message(ev->topic, ev->topic_len,
                         ev->data, ev->data_len);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error event");
        break;

    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGD(TAG, "TCP connecting...");
        break;

    default:
        break;
    }
}

void mqtt_set_mac(const char *mac)
{
    if (mac) {
        strncpy(s_mac, mac, sizeof(s_mac) - 1);
        s_mac[sizeof(s_mac) - 1] = '\0';
        snprintf(s_lwt_topic, sizeof(s_lwt_topic), "clock/%s/status", s_mac);
    }
}

static void build_lwt_cfg(esp_mqtt_client_config_t *cfg)
{
    if (s_mac[0] == '\0') return;
    cfg->session.last_will.topic = s_lwt_topic;
    cfg->session.last_will.msg = "{\"online\":false}";
    cfg->session.last_will.msg_len = 0; // null-terminated
    cfg->session.last_will.qos = 1;
    cfg->session.last_will.retain = 1;
}

esp_err_t mqtt_client_init(const char *broker_url)
{
    if (s_reconnect_timer == NULL) {
        s_reconnect_timer = xTimerCreate("mqtt_recon",
                                          pdMS_TO_TICKS(BACKOFF_BASE_MS),
                                          pdFALSE, NULL, reconnect_timer_cb);
    }

    strncpy(s_broker_url, broker_url, sizeof(s_broker_url) - 1);
    s_broker_url[sizeof(s_broker_url) - 1] = '\0';

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_broker_url,
    };

    if (s_mac[0] != '\0') {
        cfg.credentials.client_id = s_mac;
        build_lwt_cfg(&cfg);
    }

    if (client) {
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return ESP_FAIL;
    }

    esp_err_t err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                                    mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client);
        client = NULL;
        return err;
    }

    s_state = MQTT_STATE_CONNECTING;
    err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "MQTT client started, broker=%s", s_broker_url);
    return ESP_OK;
}

esp_err_t mqtt_publish_status(const char *version, const char *hw_rev,
                               const char *ip, const char *region)
{
    if (!client || s_mac[0] == '\0') return ESP_ERR_INVALID_STATE;
    char topic[128], payload[512];
    snprintf(topic, sizeof(topic), "clock/%s/status", s_mac);
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"version\":\"%s\",\"hw\":\"%s\",\"ip\":\"%s\",\"region\":\"%s\"}",
             version ? version : "0.0.0",
             hw_rev ? hw_rev : "0.1",
             ip ? ip : "0.0.0.0",
             region ? region : "");
    return esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
}

esp_err_t mqtt_publish(const char *topic, const char *json)
{
    if (!client) return ESP_ERR_INVALID_STATE;
    return esp_mqtt_client_publish(client, topic, json, 0, 1, 0);
}

bool mqtt_is_connected(void)
{
    return s_state == MQTT_STATE_CONNECTED;
}

bool mqtt_is_thin_mode(void)
{
    return s_thin_mode;
}

void mqtt_register_msg_callback(mqtt_msg_cb_t cb, void *arg)
{
    if (s_cb_count >= MAX_CALLBACKS) {
        ESP_LOGW(TAG, "Callback table full (%d max)", MAX_CALLBACKS);
        return;
    }
    s_callbacks[s_cb_count] = cb;
    s_cb_args[s_cb_count] = arg;
    s_cb_count++;
}

void mqtt_client_deinit(void)
{
    if (s_reconnect_timer) {
        xTimerStop(s_reconnect_timer, 0);
        xTimerDelete(s_reconnect_timer, 0);
        s_reconnect_timer = NULL;
    }

    if (client) {
        publish_offline_status();
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    s_state = MQTT_STATE_DISCONNECTED;
    s_thin_mode = false;
    s_fail_count = 0;
    s_cb_count = 0;
    ESP_LOGI(TAG, "MQTT client deinitialized");
}
