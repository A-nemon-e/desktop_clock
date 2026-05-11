#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "framebuffer.h"
#include "types.h"

typedef struct {
    bool show_clock;
    bool show_temp_humid;
    bool show_date;
} weather_fg_config_t;

void bg_weather_render(framebuffer_t *fb, weather_condition_t condition,
                       const weather_fg_config_t *cfg,
                       int hours, int minutes, float temp, float humidity,
                       int month, int day, int weekday);
