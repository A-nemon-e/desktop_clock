#include <unity.h>
#include <stdio.h>

static void test_clock_hms_format(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", 12, 34, 56);
    TEST_ASSERT_EQUAL_STRING("12:34:56", buf);
}

static void test_clock_hm_format(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", 12, 34);
    TEST_ASSERT_EQUAL_STRING("12:34", buf);
}

static void test_date_full_format(void)
{
    char line1[16], line2[16];
    snprintf(line1, sizeof(line1), "%04d %02d %02d", 2026, 5, 12);
    snprintf(line2, sizeof(line2), "%s", "Tuesday");
    TEST_ASSERT_EQUAL_STRING("2026 05 12", line1);
    TEST_ASSERT_EQUAL_STRING("Tuesday", line2);
}

static void test_date_compact_format(void)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d %02d %s", 5, 12, "TUE");
    TEST_ASSERT_EQUAL_STRING("05 12 TUE", buf);
}

static void test_temp_humid_format(void)
{
    // MUST use Chinese colon (U+FF1A) and space after colon for Temp
    char line1[16], line2[16];
    snprintf(line1, sizeof(line1), "Hum：%02.0f%%", 45.0);
    snprintf(line2, sizeof(line2), "Temp： %04.1f C", 25.3);
    TEST_ASSERT_EQUAL_STRING("Hum：45%", line1);
    TEST_ASSERT_EQUAL_STRING("Temp： 25.3 C", line2);
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clock_hms_format);
    RUN_TEST(test_clock_hm_format);
    RUN_TEST(test_date_full_format);
    RUN_TEST(test_date_compact_format);
    RUN_TEST(test_temp_humid_format);
    return UNITY_END();
}
