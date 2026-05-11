#include <unity.h>
#include "card_mgr.h"

// Helper: create a simple card
static card_t make_card(bg_type_t bg, fg_type_t fg_type, font_size_t font, uint8_t fg_count)
{
    card_t c = {0};
    c.bg = bg;
    c.fg_count = fg_count;
    c.relative_brightness = 0;
    c.switch_interval = 10;
    for (int i = 0; i < fg_count; i++) {
        c.fgs[i].type = fg_type;
        c.fgs[i].font = font;
        c.fgs[i].duration_ms = 5000;
    }
    return c;
}

// === BG_FIRE: only 3x5 font allowed ===
static void test_fire_accepts_3x5(void)
{
    card_t c = make_card(BG_FIRE, FG_CLOCK_HMS, FONT_3x5, 1);
    TEST_ASSERT_TRUE(card_validate(&c));
}

static void test_fire_rejects_5x5(void)
{
    card_t c = make_card(BG_FIRE, FG_CLOCK_HMS, FONT_5x5, 1);
    TEST_ASSERT_FALSE(card_validate(&c));
}

static void test_fire_rejects_7x9(void)
{
    card_t c = make_card(BG_FIRE, FG_CLOCK_HMS, FONT_7x9, 1);
    TEST_ASSERT_FALSE(card_validate(&c));
}

static void test_fire_ok_with_no_fg(void)
{
    card_t c = make_card(BG_FIRE, 0, 0, 0);
    TEST_ASSERT_TRUE(card_validate(&c));
}

// === BG_WATER_RIPPLE / HOURGLASS / LIFE: no foreground ===
static void test_water_rejects_fg(void)
{
    card_t c = make_card(BG_WATER_RIPPLE, FG_CLOCK_HMS, FONT_3x5, 1);
    TEST_ASSERT_FALSE(card_validate(&c));
}

static void test_water_ok_no_fg(void)
{
    card_t c = make_card(BG_WATER_RIPPLE, 0, 0, 0);
    TEST_ASSERT_TRUE(card_validate(&c));
}

static void test_life_rejects_fg(void)
{
    card_t c = make_card(BG_GAME_OF_LIFE, FG_CLOCK_HMS, FONT_3x5, 1);
    TEST_ASSERT_FALSE(card_validate(&c));
}

static void test_hourglass_rejects_fg(void)
{
    card_t c = make_card(BG_HOURGLASS, FG_CLOCK_HMS, FONT_3x5, 1);
    TEST_ASSERT_FALSE(card_validate(&c));
}

// === Card switching ===
static void test_card_next_rotates(void)
{
    card_mgr_init();
    // Add 3 cards
    card_t cards[3];
    for (int i = 0; i < 3; i++) cards[i] = make_card(BG_FIRE, FG_CLOCK_HMS, FONT_3x5, 1);
    // Verify circular rotation
    card_next();
    TEST_ASSERT_EQUAL_UINT8(1, card_get_index());
    card_next();
    TEST_ASSERT_EQUAL_UINT8(2, card_get_index());
    card_next();
    TEST_ASSERT_EQUAL_UINT8(0, card_get_index());
}

static void test_invalid_bg_rejected(void)
{
    card_t c = make_card((bg_type_t)99, 0, 0, 0);
    TEST_ASSERT_FALSE(card_validate(&c));
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fire_accepts_3x5);
    RUN_TEST(test_fire_rejects_5x5);
    RUN_TEST(test_fire_rejects_7x9);
    RUN_TEST(test_fire_ok_with_no_fg);
    RUN_TEST(test_water_rejects_fg);
    RUN_TEST(test_water_ok_no_fg);
    RUN_TEST(test_life_rejects_fg);
    RUN_TEST(test_hourglass_rejects_fg);
    RUN_TEST(test_card_next_rotates);
    RUN_TEST(test_invalid_bg_rejected);
    return UNITY_END();
}
