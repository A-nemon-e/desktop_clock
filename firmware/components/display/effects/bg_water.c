#include "bg_water.h"
#include <stdlib.h>
#include <string.h>

/* Fixed-point height fields (×256 for precision) */
static int16_t height[2][16][48];
static uint8_t active_buf = 0;

void bg_water_init(void)
{
    memset(height, 0, sizeof(height));
    active_buf = 0;
}

void bg_water_update(void)
{
    int cur = active_buf;
    int nxt = 1 - active_buf;

    /* Wave propagation equation (2D) */
    for (int r = 1; r < 15; r++) {
        for (int c = 1; c < 47; c++) {
            int32_t sum = (int32_t)height[cur][r - 1][c] + height[cur][r + 1][c]
                        + height[cur][r][c - 1] + height[cur][r][c + 1];
            int16_t new_h = (int16_t)((sum / 2) - height[nxt][r][c]);
            /* Damping (decay) */
            new_h -= new_h >> 6;   /* ÷ 64 */
            height[nxt][r][c] = new_h;
        }
    }

    /* Random raindrops (10% chance per frame) */
    if (rand() % 10 == 0) {
        int rx = 2 + (rand() % 44);
        int ry = 2 + (rand() % 12);
        height[nxt][ry][rx] += 512;  /* Sharp displacement */
    }

    active_buf = nxt;
}

void bg_water_render(framebuffer_t *fb)
{
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 48; c++) {
            int16_t h = height[active_buf][r][c];
            int brightness = 128 + (h >> 4);  /* Scale down from fixed-point */
            if (brightness < 0) brightness = 0;
            if (brightness > 255) brightness = 255;
            fb_set_pixel(fb, c, r, (uint8_t)brightness);
        }
    }
}
