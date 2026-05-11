#include <unity.h>
#include <string.h>
#include "framebuffer.h"

static void test_fb_init_all_zeros(void)
{
    framebuffer_init();
    framebuffer_t *fb = fb_get_back();
    for (int r = 0; r < FB_HEIGHT; r++)
        for (int c = 0; c < FB_WIDTH; c++)
            TEST_ASSERT_EQUAL_UINT8(0, fb->pixels[r][c]);
}

static void test_fb_swap_preserves_data(void)
{
    framebuffer_init();
    framebuffer_t *back = fb_get_back();
    // Write to back buffer
    fb_set_pixel(back, 24, 8, 200);
    // Swap
    fb_swap();
    // Front should now have the data
    TEST_ASSERT_EQUAL_UINT8(200, fb_front.pixels[8][24]);
    // Back should be old front (zero)
    TEST_ASSERT_EQUAL_UINT8(0, fb_back.pixels[8][24]);
}

static void test_fb_set_pixel_bounds(void)
{
    framebuffer_init();
    framebuffer_t *fb = fb_get_back();
    // In bounds
    fb_set_pixel(fb, 0, 0, 100);
    TEST_ASSERT_EQUAL_UINT8(100, fb->pixels[0][0]);
    fb_set_pixel(fb, 47, 15, 200);
    TEST_ASSERT_EQUAL_UINT8(200, fb->pixels[15][47]);
    // Out of bounds — should not crash or write
    fb_set_pixel(fb, -1, 0, 50);
    fb_set_pixel(fb, 48, 0, 50);
    fb_set_pixel(fb, 0, -1, 50);
    fb_set_pixel(fb, 0, 16, 50);
    // Verify edge pixels unchanged
    TEST_ASSERT_EQUAL_UINT8(100, fb->pixels[0][0]);
}

static void test_fb_clear(void)
{
    framebuffer_init();
    framebuffer_t *fb = fb_get_back();
    // Fill
    for (int r = 0; r < FB_HEIGHT; r++)
        for (int c = 0; c < FB_WIDTH; c++)
            fb->pixels[r][c] = 255;
    fb_clear(fb);
    // Verify all zero
    for (int r = 0; r < FB_HEIGHT; r++)
        for (int c = 0; c < FB_WIDTH; c++)
            TEST_ASSERT_EQUAL_UINT8(0, fb->pixels[r][c]);
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fb_init_all_zeros);
    RUN_TEST(test_fb_swap_preserves_data);
    RUN_TEST(test_fb_set_pixel_bounds);
    RUN_TEST(test_fb_clear);
    return UNITY_END();
}
