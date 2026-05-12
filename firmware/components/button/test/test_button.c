#include <unity.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// Simulated button event detection logic
//
// This test validates the core timing and state decisions of the
// button state machine without FreeRTOS/ESP-IDF dependencies.
//
// Constants match firmware/components/button/button.c:
//   DEBOUNCE_MS          = 50
//   DOUBLE_CLICK_MAX_MS  = 300   (max gap for second press)
//   LONG_PRESS_MIN_MS    = 800   (threshold for long-press start)
//
// Event names derived from firmware/components/shared/include/types.h:
//   BTN_EVENT_NONE             = 0
//   BTN_EVENT_SINGLE_CLICK     = 1
//   BTN_EVENT_DOUBLE_CLICK     = 2
//   BTN_EVENT_LONG_PRESS_START = 3
// ============================================================

#define DEBOUNCE_MS          50
#define DOUBLE_CLICK_MAX_MS 300
#define LONG_PRESS_MIN_MS   800

typedef enum {
    BTN_NONE = 0,
    BTN_SINGLE_CLICK,
    BTN_DOUBLE_CLICK,
    BTN_LONG_PRESS_START,
} btn_event_t;

/**
 * @brief Simplified event detection matching the real state machine.
 *
 * Decision order (same as button_task state machine):
 *  1. Press held beyond LONG_PRESS_MIN_MS → LONG_PRESS_START
 *  2. Second press within DOUBLE_CLICK_MAX_MS of previous release → DOUBLE_CLICK
 *  3. Short press (< 300 ms) not qualifying as either → SINGLE_CLICK
 *  4. Otherwise → NONE (e.g. medium-length press that was released
 *     but whose single-click hasn't timed out yet)
 */
static btn_event_t detect_event(uint32_t press_duration_ms,
                                uint32_t time_since_last_ms,
                                bool     second_press)
{
    if (press_duration_ms > LONG_PRESS_MIN_MS) {
        return BTN_LONG_PRESS_START;
    }
    if (second_press && time_since_last_ms < DOUBLE_CLICK_MAX_MS) {
        return BTN_DOUBLE_CLICK;
    }
    if (press_duration_ms < 300) {
        return BTN_SINGLE_CLICK;
    }
    return BTN_NONE;
}

// ============================================================
// Test cases
// ============================================================

static void test_single_click(void)
{
    // Short press (< 300 ms), no second click follows
    TEST_ASSERT_EQUAL(BTN_SINGLE_CLICK,
                      detect_event(200, 9999, false));
}

static void test_double_click(void)
{
    // Two presses close together (< 300 ms gap)
    TEST_ASSERT_EQUAL(BTN_DOUBLE_CLICK,
                      detect_event(200, 150, true));
}

static void test_long_press(void)
{
    // Press held beyond LONG_PRESS_MIN_MS (800 ms)
    TEST_ASSERT_EQUAL(BTN_LONG_PRESS_START,
                      detect_event(1000, 9999, false));
}

static void test_short_press_not_long(void)
{
    // Press 400 ms — released before long-press threshold;
    // should NOT produce LONG_PRESS_START
    TEST_ASSERT_NOT_EQUAL(BTN_LONG_PRESS_START,
                          detect_event(400, 9999, false));
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_single_click);
    RUN_TEST(test_double_click);
    RUN_TEST(test_long_press);
    RUN_TEST(test_short_press_not_long);
    return UNITY_END();
}
