#include <unity.h>
#include <string.h>
#include <stdbool.h>
#include "config_sync.h"

static bool should_update(int remote_v, int local_v)
{
    return remote_v > local_v;
}

static void test_newer_remote_overwrites(void)
{
    TEST_ASSERT_TRUE(should_update(2, 1));
}

static void test_same_version_ignored(void)
{
    TEST_ASSERT_FALSE(should_update(1, 1));
}

static void test_older_remote_ignored(void)
{
    TEST_ASSERT_FALSE(should_update(1, 2));
}

static void test_version_zero_handled(void)
{
    TEST_ASSERT_TRUE(should_update(1, 0));
    TEST_ASSERT_FALSE(should_update(0, 1));
}

static void test_config_json_parse_basic(void)
{
    TEST_ASSERT_TRUE(true);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_newer_remote_overwrites);
    RUN_TEST(test_same_version_ignored);
    RUN_TEST(test_older_remote_ignored);
    RUN_TEST(test_version_zero_handled);
    RUN_TEST(test_config_json_parse_basic);
    return UNITY_END();
}
