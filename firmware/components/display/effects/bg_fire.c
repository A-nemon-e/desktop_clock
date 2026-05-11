#include "bg_fire.h"
#include <stdlib.h>

static uint8_t fire[16][48];  /* 0=cold/black, 255=hot/white */

void bg_fire_init(void)
{
    for (int row = 0; row < 15; row++)
        for (int col = 0; col < 48; col++)
            fire[row][col] = 0;
    /* Bottom row = random heat */
    for (int col = 0; col < 48; col++)
        fire[15][col] = 128 + (rand() % 128);
}

void bg_fire_update(void)
{
    /* Propagate upward with averaging and decay */
    for (int row = 0; row < 15; row++) {
        for (int col = 0; col < 48; col++) {
            int sum = fire[row + 1][col];
            if (col > 0) sum += fire[row + 1][col - 1];
            if (col < 47) sum += fire[row + 1][col + 1];
            int avg = sum / 3;
            int decay = 1 + (rand() % 8);
            fire[row][col] = (avg > decay) ? avg - decay : 0;
        }
    }
    /* Randomize bottom row */
    for (int col = 0; col < 48; col++)
        fire[15][col] = 64 + (rand() % 192);
}

void bg_fire_render(framebuffer_t *fb)
{
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 48; col++)
            fb_set_pixel(fb, col, row, fire[row][col]);
}
