#pragma once
#include <stdint.h>
#include "framebuffer.h"

/* Font size identifiers (from shared/types.h: FONT_3x5=0, FONT_5x5=1, FONT_7x9=2, FONT_LONG=3) */

/* Font metrics descriptor — defined per font in its header file */
typedef struct {
    int width;              /* Pixel width of each character */
    int height;             /* Pixel height of each character */
    const uint8_t *data;    /* Pointer to packed bitmap array */
    int bytes_per_char;     /* Bytes per character glyph */
    int char_count;         /* Number of glyphs in the font */
    char start_char;        /* ASCII code of first glyph */
} font_metrics_t;

/**
 * Render a single character at (x,y) using specified font.
 * Characters are drawn with top-left at (x,y).
 * Pixels that fall outside framebuffer bounds are silently clamped.
 *
 * @param fb         Target framebuffer
 * @param x, y       Top-left pixel position
 * @param ch         ASCII character to render
 * @param font       Font size (0=FONT_3x5, 1=FONT_5x5, 2=FONT_7x9)
 * @param brightness 0-255 grayscale intensity
 * @return           Width in pixels of rendered character (for advancing x)
 */
int render_char(framebuffer_t *fb, int x, int y, char ch, int font, uint8_t brightness);

/**
 * Render a null-terminated string starting at (x,y).
 * Advances x position by character width + 1 pixel spacing.
 * Space characters advance without rendering.
 * Characters that fall outside framebuffer are drawn (clamped per-pixel).
 *
 * @return Total width used (for centering calculations)
 */
int render_text(framebuffer_t *fb, int x, int y, const char *text, int font, uint8_t brightness);

/**
 * Get pixel width of a single character in the given font.
 */
int font_char_width(int font);

/**
 * Get pixel height of a single character in the given font.
 */
int font_char_height(int font);
