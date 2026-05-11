#include "bg_code_rain.h"
#include <stdlib.h>
#include <string.h>

#define MAX_DROPS 24

typedef struct {
    int8_t col;
    int8_t row;
    int8_t speed;
    uint8_t length;
    uint8_t brightness;
} raindrop_t;

static raindrop_t drops[MAX_DROPS];
static uint8_t screen_chars[16][48];  /* Random character "glyph" (brightness value) */

void bg_code_rain_init(void)
{
    memset(drops, 0, sizeof(drops));
    /* Initialize raindrops at random positions */
    int count = 16 + (rand() % 8);
    for (int i = 0; i < count; i++) {
        drops[i].col = rand() % 48;
        drops[i].row = -(rand() % 16);  /* Start above screen */
        drops[i].speed = 2 + (rand() % 3);
        drops[i].length = 4 + (rand() % 6);
        drops[i].brightness = 200 + (rand() % 56);
    }
    /* Random character "glyphs" */
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 48; c++)
            screen_chars[r][c] = 128 + (rand() % 128);
}

void bg_code_rain_update(void)
{
    for (int i = 0; i < MAX_DROPS; i++) {
        if (drops[i].speed == 0) continue;
        drops[i].row += drops[i].speed;
        if (drops[i].row > 15 + drops[i].length) {
            /* Reset drop to top */
            drops[i].col = rand() % 48;
            drops[i].row = -(rand() % 8);
            drops[i].speed = 2 + (rand() % 3);
            drops[i].length = 4 + (rand() % 6);
        }
    }
    /* Occasionally add new drops */
    if (rand() % 20 == 0) {
        for (int i = 0; i < MAX_DROPS; i++) {
            if (drops[i].speed == 0) {
                drops[i].col = rand() % 48;
                drops[i].row = -(rand() % 8);
                drops[i].speed = 2 + (rand() % 3);
                drops[i].length = 4 + (rand() % 6);
                drops[i].brightness = 200 + (rand() % 56);
                break;
            }
        }
    }
}

static bool is_masked(const rect_t *mask, int count, int x, int y)
{
    for (int i = 0; i < count; i++) {
        if (x >= mask[i].x && x < mask[i].x + mask[i].w &&
            y >= mask[i].y && y < mask[i].y + mask[i].h)
            return true;
    }
    return false;
}

void bg_code_rain_render(framebuffer_t *fb, const rect_t *mask, int mask_count)
{
    /* Clear buffer */
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 48; c++)
            fb->pixels[r][c] = 0;

    /* Render rain drops */
    for (int i = 0; i < MAX_DROPS; i++) {
        if (drops[i].speed == 0) continue;
        int col = drops[i].col;
        if (col < 0 || col >= 48) continue;

        /* Draw head (brightest) */
        int head_row = drops[i].row;
        if (head_row >= 0 && head_row < 16 && !is_masked(mask, mask_count, col, head_row))
            fb_set_pixel(fb, col, head_row, 255);

        /* Draw tail (fading) */
        for (int t = 1; t <= drops[i].length; t++) {
            int tr = head_row - t;
            if (tr >= 0 && tr < 16 && !is_masked(mask, mask_count, col, tr)) {
                int b = 255 - (t * 255 / drops[i].length);
                if (b < 0) b = 0;
                fb_set_pixel(fb, col, tr, (uint8_t)b);
            }
        }
    }
}
