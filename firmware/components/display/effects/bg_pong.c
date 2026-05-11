#include "bg_pong.h"
#include "font_renderer.h"
#include <string.h>
#include <stdio.h>

static int ball_x, ball_y, ball_dx, ball_dy;
static int pad_left_y, pad_right_y;
static int g_hours, g_minutes, g_seconds;

void bg_pong_init(void)
{
    ball_x = 23;
    ball_y = 7;
    ball_dx = 1;
    ball_dy = 1;
    pad_left_y = 5;
    pad_right_y = 5;
    g_hours = 0;
    g_minutes = 0;
    g_seconds = 0;
}

void bg_pong_update(int hours, int minutes, int seconds)
{
    g_hours = hours;
    g_minutes = minutes;
    g_seconds = seconds;

    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_y <= 0 || ball_y >= 15)
        ball_dy = -ball_dy;

    if (ball_x <= 2 && ball_y >= pad_left_y && ball_y <= pad_left_y + 2)
        ball_dx = 1;
    if (ball_x >= 45 && ball_y >= pad_right_y && ball_y <= pad_right_y + 2)
        ball_dx = -1;

    if (ball_x < 0) {
        ball_x = 23;
        ball_y = 7;
        ball_dx = 1;
    }
    if (ball_x > 47) {
        ball_x = 23;
        ball_y = 7;
        ball_dx = -1;
    }

    pad_left_y = (hours * 16) / 23;
    pad_right_y = (minutes * 16) / 59;
}

void bg_pong_render(framebuffer_t *fb, const pong_fg_config_t *cfg,
                    float temp, float humidity, int month, int day, int weekday)
{
    int i;

    memset(fb->pixels, 0, sizeof(fb->pixels));

    for (i = 0; i < 3; i++) {
        fb_set_pixel(fb, 1, pad_left_y + i, 200);
        fb_set_pixel(fb, 46, pad_right_y + i, 200);
    }

    fb_set_pixel(fb, ball_x, ball_y, 255);

    for (i = 0; i < 16; i += 2)
        fb_set_pixel(fb, 23, i, 100);

    char buf[16];

    if (cfg->show_clock) {
        snprintf(buf, sizeof(buf), "%02d", g_hours);
        render_text(fb, 3, 3, buf, 0, 255);
        snprintf(buf, sizeof(buf), "%02d", g_minutes);
        render_text(fb, 27, 3, buf, 0, 255);

        int sec_len = g_seconds * 48 / 60;
        for (i = 0; i < sec_len; i++) {
            int b = 255 - (i > 0 ? (i * 255 / sec_len) : 0);
            fb_set_pixel(fb, i, 15, b > 0 ? b : 32);
        }
    }

    if (cfg->show_temp_humid) {
        snprintf(buf, sizeof(buf), "%02dC", (int)temp);
        render_text(fb, 1, 8, buf, 0, 200);
        snprintf(buf, sizeof(buf), "%02d%%", (int)humidity);
        render_text(fb, 28, 8, buf, 0, 200);
    }

    if (cfg->show_date) {
        const char *wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
        snprintf(buf, sizeof(buf), "%02d %02d %s", month, day, wd[weekday]);
        int tw = (int)strlen(buf) * 4;
        render_text(fb, 24 - tw/2, 12, buf, 0, 200);
    }
}
