#include "bg_hourglass.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PARTICLES 50
#define HOURGLASS_TOP_Y    2
#define HOURGLASS_BOTTOM_Y 13
#define HOURGLASS_CENTER_X 24
#define HOURGLASS_NECK_Y   7
#define HOURGLASS_WIDTH    20
#define HOURGLASS_NECK_W   3

typedef struct { float x, y; } particle_t;
static particle_t particles[MAX_PARTICLES];
static int particle_count = MAX_PARTICLES;
static bool flowing_down = true;  // true=flowing to bottom, false=flowing to top

void bg_hourglass_init(void)
{
    // Initialize particles in upper chamber
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x = HOURGLASS_CENTER_X - HOURGLASS_WIDTH/2.0f + (rand() % HOURGLASS_WIDTH) + 0.5f;
        particles[i].y = HOURGLASS_TOP_Y + (rand() % (HOURGLASS_NECK_Y - HOURGLASS_TOP_Y));
    }
}

void bg_hourglass_update(void)
{
    bool all_settled = true;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        float *py = &particles[i].y;
        float *px = &particles[i].x;

        // Gravity
        if (flowing_down) *py += 0.3f;
        else *py -= 0.3f;

        // Horizontal drift
        *px += ((float)(rand() % 100) / 100.0f - 0.5f) * 0.4f;

        // Neck constraint
        if (flowing_down) {
            float neck_hw = HOURGLASS_NECK_W / 2.0f;
            if (*py >= HOURGLASS_NECK_Y && *py <= HOURGLASS_NECK_Y + 1) {
                if (*px < HOURGLASS_CENTER_X - neck_hw) *px = HOURGLASS_CENTER_X - neck_hw;
                if (*px > HOURGLASS_CENTER_X + neck_hw) *px = HOURGLASS_CENTER_X + neck_hw;
            }
        }

        // Wall constraints
        float half_w = HOURGLASS_WIDTH / 2.0f;
        if (*px < HOURGLASS_CENTER_X - half_w) *px = HOURGLASS_CENTER_X - half_w;
        if (*px > HOURGLASS_CENTER_X + half_w) *px = HOURGLASS_CENTER_X + half_w;

        // Floor/ceiling
        if (flowing_down && *py >= HOURGLASS_BOTTOM_Y) {
            *py = HOURGLASS_BOTTOM_Y;
        } else if (!flowing_down && *py <= HOURGLASS_TOP_Y) {
            *py = HOURGLASS_TOP_Y;
        } else {
            all_settled = false;
        }
    }

    // Flip if all settled
    if (all_settled) {
        flowing_down = !flowing_down;
    }
}

void bg_hourglass_render(framebuffer_t *fb)
{
    // Clear
    memset(fb->pixels, 0, sizeof(fb->pixels));

    // Draw hourglass outline
    int hw = HOURGLASS_WIDTH / 2;
    int nhw = HOURGLASS_NECK_W / 2;
    int cx = HOURGLASS_CENTER_X;
    int top = HOURGLASS_TOP_Y, neck = HOURGLASS_NECK_Y, bot = HOURGLASS_BOTTOM_Y;

    // Upper triangle
    for (int y = top; y <= neck; y++) {
        int w = hw - (hw - nhw) * (y - top) / (neck - top);
        for (int x = cx - w; x <= cx + w; x++) {
            if (x == cx - w || x == cx + w || y == top || y == neck)
                fb_set_pixel(fb, x, y, 180);
        }
    }
    // Lower triangle
    for (int y = neck; y <= bot; y++) {
        int w = nhw + (hw - nhw) * (y - neck) / (bot - neck);
        for (int x = cx - w; x <= cx + w; x++) {
            if (x == cx - w || x == cx + w || y == neck || y == bot)
                fb_set_pixel(fb, x, y, 180);
        }
    }

    // Draw particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        int px = (int)particles[i].x;
        int py = (int)particles[i].y;
        if (px >= 0 && px < 48 && py >= 0 && py < 16) {
            fb_set_pixel(fb, px, py, 255);
        }
    }
}
