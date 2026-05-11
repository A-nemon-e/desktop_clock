#include "bg_life.h"
#include <stdlib.h>
#include <string.h>

static uint8_t grid[2][16][48];  // Double-buffered: 0=dead, 1=alive
static uint8_t cur = 0;
static uint8_t age[16][48];       // Age per cell (for grayscale aging effect)
static uint8_t frame_skip = 0;

void bg_life_init(uint32_t seed)
{
    srand(seed);
    memset(grid, 0, sizeof(grid));
    memset(age, 0, sizeof(age));
    cur = 0;

    // Random 30% density
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 48; c++) {
            if ((rand() % 100) < 30) {
                grid[cur][r][c] = 1;
                age[r][c] = 1;
            }
        }
    }
}

static int count_neighbors(int row, int col)
{
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = row + dr, nc = col + dc;
            if (nr >= 0 && nr < 16 && nc >= 0 && nc < 48) {
                count += grid[cur][nr][nc];
            }
        }
    }
    return count;
}

void bg_life_update(void)
{
    // Only update every 3 frames (~10 gen/sec at 30fps)
    frame_skip++;
    if (frame_skip < 3) return;
    frame_skip = 0;

    int nxt = 1 - cur;
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 48; c++) {
            int n = count_neighbors(r, c);
            if (grid[cur][r][c]) {
                // Alive: survives with 2 or 3 neighbors
                grid[nxt][r][c] = (n == 2 || n == 3) ? 1 : 0;
            } else {
                // Dead: born with exactly 3 neighbors
                grid[nxt][r][c] = (n == 3) ? 1 : 0;
            }
            // Age tracking
            if (grid[nxt][r][c]) age[r][c]++;
            else age[r][c] = 0;
        }
    }
    cur = nxt;
}

void bg_life_render(framebuffer_t *fb)
{
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 48; c++) {
            if (grid[cur][r][c]) {
                // Brightness based on age: older = brighter (128 + age)
                int b = 128 + (age[r][c] % 128);
                fb_set_pixel(fb, c, r, b);
            } else {
                fb_set_pixel(fb, c, r, 0);
            }
        }
    }
}
