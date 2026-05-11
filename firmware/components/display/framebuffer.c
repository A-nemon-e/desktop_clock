#include "framebuffer.h"
#include <string.h>

framebuffer_t fb_front = {0};
framebuffer_t fb_back = {0};
SemaphoreHandle_t fb_mutex = NULL;

void framebuffer_init(void)
{
    fb_mutex = xSemaphoreCreateMutex();
    memset(&fb_front, 0, sizeof(fb_front));
    memset(&fb_back, 0, sizeof(fb_back));
}

framebuffer_t* fb_get_back(void)
{
    return &fb_back;
}

void fb_swap(void)
{
    if (xSemaphoreTake(fb_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        framebuffer_t temp = fb_front;
        fb_front = fb_back;
        fb_back = temp;
        xSemaphoreGive(fb_mutex);
    }
}

void fb_clear(framebuffer_t *fb)
{
    memset(fb->pixels, 0, sizeof(fb->pixels));
}

void fb_set_pixel(framebuffer_t *fb, int x, int y, uint8_t brightness)
{
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fb->pixels[y][x] = brightness;
    }
}
