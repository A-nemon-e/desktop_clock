#include "fg_render.h"
#include "font_renderer.h"
#include <stdio.h>

void fg_clock_hms(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                  int h, int m, int s)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    render_text(fb, x, y, buf, font, brightness);
}

void fg_clock_hm(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                 int h, int m)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    render_text(fb, x, y, buf, font, brightness);
}

void fg_date_full(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                  int year, int month, int day, int weekday)
{
    const char *wd[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    char line1[16], line2[16];
    snprintf(line1, sizeof(line1), "%04d %02d %02d", year, month, day);
    snprintf(line2, sizeof(line2), "%s", wd[weekday]);
    render_text(fb, x, y, line1, font, brightness);
    render_text(fb, x + 2, y + font_char_height(font) + 1, line2, font, brightness);
}

void fg_date_compact(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                     int month, int day, int weekday)
{
    const char *wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d %02d %s", month, day, wd[weekday]);
    render_text(fb, x, y, buf, font, brightness);
}

void fg_temp_humid(framebuffer_t *fb, int x, int y, int font, uint8_t brightness,
                   float temp, float humidity)
{
    char line1[16], line2[16];
    snprintf(line1, sizeof(line1), "Hum：%02.0f%%", humidity);
    snprintf(line2, sizeof(line2), "Temp： %04.1f C", temp);
    render_text(fb, x, y, line1, font, brightness);
    int line_h = font_char_height(font) + 1;
    render_text(fb, x, y + line_h, line2, font, brightness);
}
