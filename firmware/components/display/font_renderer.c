#include "font_renderer.h"
#include "fonts/font_3x5.h"
#include "fonts/font_5x5.h"
#include "fonts/font_7x9.h"

static const font_metrics_t *get_font(int font_type)
{
    switch (font_type) {
        case 0: return &font_3x5_metrics;
        case 1: return &font_5x5_metrics;
        case 2: return &font_7x9_metrics;
        default: return &font_3x5_metrics;
    }
}

int font_char_width(int font)
{
    return get_font(font)->width;
}

int font_char_height(int font)
{
    return get_font(font)->height;
}

int render_char(framebuffer_t *fb, int x, int y, char ch, int font_type, uint8_t brightness)
{
    const font_metrics_t *fm = get_font(font_type);

    int idx = -1;
    if (ch >= fm->start_char && ch < fm->start_char + fm->char_count) {
        idx = ch - fm->start_char;
    }
    if (idx < 0) {
        return fm->width;
    }

    const uint8_t *char_data = fm->data + idx * fm->bytes_per_char;

    int bit_idx = 0;
    for (int dy = 0; dy < fm->height; dy++) {
        for (int dx = 0; dx < fm->width; dx++) {
            int byte_off = bit_idx / 8;
            int bit_off = bit_idx % 8;
            if (byte_off < fm->bytes_per_char) {
                uint8_t byte = char_data[byte_off];
                if (byte & (1 << bit_off)) {
                    fb_set_pixel(fb, x + dx, y + dy, brightness);
                }
            }
            bit_idx++;
        }
    }

    return fm->width;
}

int render_text(framebuffer_t *fb, int x, int y, const char *text, int font_type, uint8_t brightness)
{
    int cx = x;
    const font_metrics_t *fm = get_font(font_type);

    while (*text) {
        if (*text == ' ') {
            cx += fm->width + 1;
        } else {
            cx += render_char(fb, cx, y, *text, font_type, brightness) + 1;
        }
        text++;
    }

    return cx - x;
}
