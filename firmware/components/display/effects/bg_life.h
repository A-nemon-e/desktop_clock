#pragma once
#include <stdint.h>
#include "framebuffer.h"

void bg_life_init(uint32_t seed);
void bg_life_update(void);  // Call every 3 frames (~10 generations/sec)
void bg_life_render(framebuffer_t *fb);
