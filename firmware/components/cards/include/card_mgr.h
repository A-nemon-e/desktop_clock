#pragma once
#include "esp_err.h"
#include "types.h"

/**
 * @file card_mgr.h
 * @brief Card manager — rotation, validation, NVS persistence, brightness control.
 *
 * Cards are "slides" in the display rotation. Each card specifies a background
 * effect, 0-5 foreground layers, relative brightness offset, and auto-switch interval.
 *
 * Configuration layout:
 *   Instant size = sizeof(card_t) × MAX_CARDS = ~140 bytes × 9 = ~1260 bytes
 *   NVS serialization: JSON string under "cards" namespace, key "data"
 *
 * Constraint validation (card_validate):
 *   FIRE bg        → all foregrounds must use FONT_3x5
 *   WATER/HOURGLASS/LIFE → no foregrounds allowed
 *
 * Button integration:
 *   DOUBLE_CLICK → card_next()
 *   LONG_PRESS   → brightness_adjust()
 */

esp_err_t card_mgr_init(void);

void card_next(void);
void card_set(uint8_t idx);

bool card_validate(const card_t *card);

void card_mgr_save_to_nvs(void);
void card_mgr_load_from_nvs(void);

card_t *card_get_current(void);
uint8_t card_get_count(void);
uint8_t card_get_index(void);

uint8_t brightness_get_gcc(void);
void brightness_adjust(bool increase);
void brightness_set(uint8_t gcc);

bool card_mgr_is_display_on(void);
void card_mgr_toggle_display(void);
