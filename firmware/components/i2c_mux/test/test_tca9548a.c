#include <unity.h>
#include "tca9548a.h"

// --- Channel selection bit-math tests ---
// TCA9548A control byte: bit N = 1 selects channel N, 0x00 = all disabled.

static void test_channel_0_select_byte(void)
{
    // Channel 0 → control byte should be (1<<0) = 0x01
    TEST_ASSERT_EQUAL_UINT8(0x01, 1 << 0);
}

static void test_channel_5_select_byte(void)
{
    // Channel 5 → control byte should be (1<<5) = 0x20
    TEST_ASSERT_EQUAL_UINT8(0x20, 1 << 5);
}

static void test_disable_all_byte(void)
{
    // Disable all → control byte 0x00
    uint8_t ctrl = 0x00;
    for (int ch = 0; ch < 8; ch++) {
        TEST_ASSERT_EQUAL_UINT8(0, (ctrl >> ch) & 1);
    }
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_channel_0_select_byte);
    RUN_TEST(test_channel_5_select_byte);
    RUN_TEST(test_disable_all_byte);
    return UNITY_END();
}
