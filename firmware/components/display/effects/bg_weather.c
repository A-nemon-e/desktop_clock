#include "bg_weather.h"
#include "font_renderer.h"
#include <stdio.h>
#include <string.h>

static void draw_sun(framebuffer_t *fb)
{
    int cx = 12, cy = 5, r, a;
    for (r = 0; r < 3; r++)
        for (a = 0; a < 8; a++)
            fb_set_pixel(fb, cx + r*1, cy + r*0, 200);
    for (a = 0; a < 8; a++)
        fb_set_pixel(fb, cx + 4, cy + 3, 128);
}

static void draw_cloud(framebuffer_t *fb, int cx, int cy)
{
    int r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 6; c++)
            if (r < 2 || c > 1)
                fb_set_pixel(fb, cx + c, cy + r, 160);
}

static void draw_rain(framebuffer_t *fb)
{
    int i, j;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 3; j++)
            fb_set_pixel(fb, 4 + i*5, 8 + j*2, 200);
}

void bg_weather_render(framebuffer_t *fb, weather_condition_t condition,
                       const weather_fg_config_t *cfg,
                       int hours, int minutes, float temp, float humidity,
                       int month, int day, int weekday)
{
    memset(fb->pixels, 0, sizeof(fb->pixels));

    switch (condition) {
        case WEATHER_SUNNY:
            draw_sun(fb);
            break;
        case WEATHER_CLOUDY:
            draw_cloud(fb, 2, 3);
            break;
        case WEATHER_RAIN:
            draw_cloud(fb, 2, 3);
            draw_rain(fb);
            break;
        case WEATHER_SNOW:
            fb_set_pixel(fb, 10, 6, 255);
            break;
        case WEATHER_OVERCAST:
            draw_cloud(fb, 2, 3);
            break;
        case WEATHER_WINDY:
            break;
        default:
            break;
    }

    char buf[16];

    if (cfg->show_clock) {
        snprintf(buf, sizeof(buf), "%02d %02d", hours, minutes);
        int tw = (int)strlen(buf) * 4;
        render_text(fb, 48 - tw - 4, 4, buf, 0, 255);
    }

    if (cfg->show_temp_humid) {
        snprintf(buf, sizeof(buf), "%02.0fC", temp);
        render_text(fb, 24, 8, buf, 0, 220);
        snprintf(buf, sizeof(buf), "%02.0f%%", humidity);
        render_text(fb, 38, 8, buf, 0, 220);
    }

    if (cfg->show_date) {
        const char *wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
        snprintf(buf, sizeof(buf), "%02d %02d %s", month, day, wd[weekday]);
        int tw = (int)strlen(buf) * 3;
        render_text(fb, 48 - tw - 1, 14, buf, 0, 180);
    }
}
