#include "card_mgr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define TAG "CRD"
#define NVS_NAMESPACE "cards"
#define NVS_KEY_DATA  "data"
#define GCC_MIN  5
#define GCC_MAX  127
#define GCC_STEP 5
#define GCC_DEFAULT 0x3F

static card_t s_cards[MAX_CARDS];
static uint8_t s_card_count = 0;
static uint8_t s_current_idx = 0;
static uint8_t s_gcc_value = GCC_DEFAULT;
static bool s_display_on = true;

static bool validate_fg_font(const card_t *card)
{
    for (int i = 0; i < card->fg_count; i++) {
        if (card->fgs[i].font != FONT_3x5) {
            return false;
        }
    }
    return true;
}

bool card_validate(const card_t *card)
{
    if (!card) return false;

    if (card->bg >= BG_COUNT) return false;
    if (card->fg_count > MAX_FG_PER_CARD) return false;

    switch (card->bg) {
    case BG_FIRE:
        return card->fg_count == 0 || validate_fg_font(card);
    case BG_WATER_RIPPLE:
    case BG_HOURGLASS:
    case BG_GAME_OF_LIFE:
        return card->fg_count == 0;
    case BG_CODE_RAIN:
    case BG_PONG_CLOCK:
    case BG_WEATHER:
        return true;
    default:
        return false;
    }
}

static void set_default_card(void)
{
    memset(s_cards, 0, sizeof(s_cards));
    s_cards[0].bg = BG_FIRE;
    s_cards[0].fgs[0].type = FG_CLOCK_HMS;
    s_cards[0].fgs[0].font = FONT_3x5;
    s_cards[0].fgs[0].duration_ms = 0;
    s_cards[0].fg_count = 1;
    s_cards[0].relative_brightness = 0;
    s_cards[0].switch_interval = 0;
    s_card_count = 1;
    s_current_idx = 0;
}

static cJSON *card_to_json(const card_t *card)
{
    cJSON *c = cJSON_CreateObject();
    cJSON_AddNumberToObject(c, "bg", card->bg);
    cJSON_AddNumberToObject(c, "fc", card->fg_count);
    cJSON_AddNumberToObject(c, "rb", card->relative_brightness);
    cJSON_AddNumberToObject(c, "sw", card->switch_interval);

    cJSON *fgs = cJSON_AddArrayToObject(c, "fgs");
    for (int i = 0; i < card->fg_count; i++) {
        cJSON *f = cJSON_CreateObject();
        cJSON_AddNumberToObject(f, "t", card->fgs[i].type);
        cJSON_AddNumberToObject(f, "f", card->fgs[i].font);
        cJSON_AddNumberToObject(f, "d", card->fgs[i].duration_ms);
        cJSON_AddItemToArray(fgs, f);
    }
    return c;
}

static void json_to_card(cJSON *c, card_t *card)
{
    memset(card, 0, sizeof(card_t));
    card->bg = (bg_type_t)cJSON_GetObjectItem(c, "bg")->valueint;
    card->fg_count = (uint8_t)cJSON_GetObjectItem(c, "fc")->valueint;
    card->relative_brightness = (int8_t)cJSON_GetObjectItem(c, "rb")->valueint;
    card->switch_interval = (uint16_t)cJSON_GetObjectItem(c, "sw")->valueint;

    if (card->fg_count > MAX_FG_PER_CARD) card->fg_count = MAX_FG_PER_CARD;

    cJSON *fgs = cJSON_GetObjectItem(c, "fgs");
    if (fgs && cJSON_IsArray(fgs)) {
        int fg_len = cJSON_GetArraySize(fgs);
        if (fg_len > card->fg_count) fg_len = card->fg_count;
        for (int i = 0; i < fg_len; i++) {
            cJSON *f = cJSON_GetArrayItem(fgs, i);
            if (f) {
                card->fgs[i].type = (fg_type_t)cJSON_GetObjectItem(f, "t")->valueint;
                card->fgs[i].font = (font_size_t)cJSON_GetObjectItem(f, "f")->valueint;
                card->fgs[i].duration_ms = (uint16_t)cJSON_GetObjectItem(f, "d")->valueint;
            }
        }
    }
}

void card_mgr_save_to_nvs(void)
{
    nvs_handle_t n;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &n) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for write");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "v", 1);
    cJSON_AddNumberToObject(root, "gcc", s_gcc_value);
    cJSON_AddNumberToObject(root, "idx", s_current_idx);

    cJSON *arr = cJSON_AddArrayToObject(root, "cards");
    for (int i = 0; i < s_card_count; i++) {
        cJSON_AddItemToArray(arr, card_to_json(&s_cards[i]));
    }

    char *str = cJSON_PrintUnformatted(root);
    if (str) {
        nvs_set_str(n, NVS_KEY_DATA, str);
        nvs_commit(n);
        free(str);
        ESP_LOGI(TAG, "Saved %d cards to NVS (len=%zu)", s_card_count, strlen(str));
    }
    cJSON_Delete(root);
    nvs_close(n);
}

void card_mgr_load_from_nvs(void)
{
    nvs_handle_t n;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &n) != ESP_OK) {
        ESP_LOGI(TAG, "No saved cards in NVS, using defaults");
        return;
    }

    size_t len = 0;
    if (nvs_get_str(n, NVS_KEY_DATA, NULL, &len) != ESP_OK || len == 0) {
        nvs_close(n);
        return;
    }

    char *buf = malloc(len);
    if (!buf) {
        nvs_close(n);
        return;
    }

    if (nvs_get_str(n, NVS_KEY_DATA, buf, &len) != ESP_OK) {
        free(buf);
        nvs_close(n);
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        nvs_close(n);
        return;
    }

    cJSON *v = cJSON_GetObjectItem(root, "v");
    if (v) {
        ESP_LOGI(TAG, "Loading cards (config version %d)", v->valueint);
    }

    cJSON *gcc = cJSON_GetObjectItem(root, "gcc");
    if (gcc) {
        s_gcc_value = (uint8_t)gcc->valueint;
        if (s_gcc_value < GCC_MIN) s_gcc_value = GCC_MIN;
        if (s_gcc_value > GCC_MAX) s_gcc_value = GCC_MAX;
    }

    cJSON *idx = cJSON_GetObjectItem(root, "idx");
    if (idx) {
        s_current_idx = (uint8_t)idx->valueint;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "cards");
    if (arr && cJSON_IsArray(arr)) {
        s_card_count = (uint8_t)cJSON_GetArraySize(arr);
        if (s_card_count > MAX_CARDS) s_card_count = MAX_CARDS;

        for (int i = 0; i < s_card_count; i++) {
            cJSON *c = cJSON_GetArrayItem(arr, i);
            if (c) json_to_card(c, &s_cards[i]);
        }

        if (s_current_idx >= s_card_count) s_current_idx = 0;
    }

    cJSON_Delete(root);
    nvs_close(n);
    ESP_LOGI(TAG, "Loaded %d cards from NVS, gcc=%d, idx=%d",
             s_card_count, s_gcc_value, s_current_idx);
}

esp_err_t card_mgr_init(void)
{
    card_mgr_load_from_nvs();

    if (s_card_count == 0) {
        set_default_card();
        card_mgr_save_to_nvs();
    }

    ESP_LOGI(TAG, "Card manager ready: %d cards, current=%d, gcc=%d",
             s_card_count, s_current_idx, s_gcc_value);
    return ESP_OK;
}

void card_next(void)
{
    if (s_card_count == 0) return;
    s_current_idx = (s_current_idx + 1) % s_card_count;
    ESP_LOGI(TAG, "Switched to card %d/%d", s_current_idx, s_card_count);
}

void card_set(uint8_t idx)
{
    if (idx < s_card_count) {
        s_current_idx = idx;
        ESP_LOGI(TAG, "Set card index to %d", idx);
    }
}

card_t *card_get_current(void)
{
    if (s_card_count == 0) return NULL;
    return &s_cards[s_current_idx];
}

uint8_t card_get_count(void) { return s_card_count; }
uint8_t card_get_index(void) { return s_current_idx; }

uint8_t brightness_get_gcc(void) { return s_gcc_value; }

void brightness_adjust(bool increase)
{
    if (increase) {
        if (s_gcc_value < GCC_MAX) {
            s_gcc_value += GCC_STEP;
            if (s_gcc_value > GCC_MAX) s_gcc_value = GCC_MAX;
            ESP_LOGI(TAG, "Brightness up: %d", s_gcc_value);
        }
    } else {
        if (s_gcc_value > GCC_MIN) {
            s_gcc_value -= GCC_STEP;
            if (s_gcc_value < GCC_MIN) s_gcc_value = GCC_MIN;
            ESP_LOGI(TAG, "Brightness down: %d", s_gcc_value);
        }
    }
}

void brightness_set(uint8_t gcc)
{
    if (gcc >= GCC_MIN && gcc <= GCC_MAX) {
        s_gcc_value = gcc;
    }
}

bool card_mgr_is_display_on(void) { return s_display_on; }

void card_mgr_toggle_display(void)
{
    s_display_on = !s_display_on;
    ESP_LOGI(TAG, "Display %s", s_display_on ? "ON" : "OFF");
}
