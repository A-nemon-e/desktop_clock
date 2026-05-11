#pragma once
#include "framebuffer.h"

void bg_water_init(void);
void bg_water_update(void);               /* Call every frame */
void bg_water_render(framebuffer_t *fb);  /* Write to back buffer */
