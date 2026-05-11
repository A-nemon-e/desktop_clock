#pragma once
#include "framebuffer.h"

void bg_fire_init(void);
void bg_fire_update(void);               /* Call every frame */
void bg_fire_render(framebuffer_t *fb);  /* Write to back buffer */
