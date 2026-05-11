#pragma once
#include <stdint.h>
#include "framebuffer.h"

void fg_clock_hms(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                  int hours, int minutes, int seconds);

void fg_clock_hm(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                 int hours, int minutes);

void fg_date_full(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                  int year, int month, int day, int weekday);

void fg_date_compact(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                     int month, int day, int weekday);

void fg_temp_humid(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                   float temperature, float humidity);
