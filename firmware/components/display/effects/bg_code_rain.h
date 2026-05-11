#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "framebuffer.h"

typedef struct {
    int x, y, w, h;
} rect_t;

void bg_code_rain_init(void);
void bg_code_rain_update(void);
void bg_code_rain_render(framebuffer_t *fb, const rect_t *mask, int mask_count);
