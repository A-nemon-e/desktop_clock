#include <unity.h>
#include "font_renderer.h"
#include "framebuffer.h"

void setUp(void) { framebuffer_init(); }
void tearDown(void) {}

static void test_3x5_metrics(void)
{
    TEST_ASSERT_EQUAL_INT(3, font_char_width(FONT_3x5));
    TEST_ASSERT_EQUAL_INT(5, font_char_height(FONT_3x5));
}

static void test_5x5_metrics(void)
{
    TEST_ASSERT_EQUAL_INT(5, font_char_width(FONT_5x5));
    TEST_ASSERT_EQUAL_INT(5, font_char_height(FONT_5x5));
}

static void test_7x9_metrics(void)
{
    TEST_ASSERT_EQUAL_INT(7, font_char_width(FONT_7x9));
    TEST_ASSERT_EQUAL_INT(9, font_char_height(FONT_7x9));
}

static void test_render_char_3x5(void)
{
    framebuffer_t *fb = fb_get_back();
    int w = render_char(fb, 0, 0, '0', FONT_3x5, 255);
    TEST_ASSERT_EQUAL_INT(3, w);
    int count = 0;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (fb->pixels[r][c] == 255) count++;
    TEST_ASSERT_GREATER_THAN_INT(0, count);
}

static void test_render_text_returns_width(void)
{
    framebuffer_t *fb = fb_get_back();
    int w = render_text(fb, 0, 0, "88", FONT_3x5, 255);
    TEST_ASSERT_GREATER_THAN_INT(5, w);
}

static void test_out_of_bounds_no_crash(void)
{
    framebuffer_t *fb = fb_get_back();
    render_char(fb, 46, 0, 'M', FONT_7x9, 255);
    render_char(fb, 0, 14, 'M', FONT_7x9, 255);
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_3x5_metrics);
    RUN_TEST(test_5x5_metrics);
    RUN_TEST(test_7x9_metrics);
    RUN_TEST(test_render_char_3x5);
    RUN_TEST(test_render_text_returns_width);
    RUN_TEST(test_out_of_bounds_no_crash);
    return UNITY_END();
}
