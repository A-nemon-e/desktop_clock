#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "framebuffer.h"

typedef struct {
    bool show_clock;       // Show HH MM (left/right halves) + seconds bar
    bool show_temp_humid;  // Show temp (left) / humidity (right)
    bool show_date;        // Show date centered
} pong_fg_config_t;

void bg_pong_init(void);
void bg_pong_update(int hours, int minutes, int seconds);
void bg_pong_render(framebuffer_t *fb, const pong_fg_config_t *cfg,
                    float temp, float humidity, int month, int day, int weekday);
