#include <unity.h>
#include <string.h>
#include "geo_pipeline.h"

static void test_geo_info_zeroed_on_init(void)
{
    geo_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL_STRING("", info.public_ip);
    TEST_ASSERT_EQUAL_STRING("", info.region);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, info.latitude);
}

static void test_public_ip_buf_has_space(void)
{
    geo_info_t info;
    memset(&info, 0, sizeof(info));
    // IPv4 max length: 15 chars + null; buf is 46 bytes for IPv6 support
    strncpy(info.public_ip, "255.255.255.255", sizeof(info.public_ip));
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", info.public_ip);
}

static void test_region_fields_accept_chinese(void)
{
    geo_info_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.region, "北京市", sizeof(info.region));
    strncpy(info.city, "朝阳区", sizeof(info.city));
    strncpy(info.timezone, "Asia/Shanghai", sizeof(info.timezone));
    TEST_ASSERT_EQUAL_STRING("北京市", info.region);
    TEST_ASSERT_EQUAL_STRING("朝阳区", info.city);
    TEST_ASSERT_EQUAL_STRING("Asia/Shanghai", info.timezone);
}

static void test_struct_size_fits_stack(void)
{
    TEST_ASSERT_LESS_THAN_INT(512, sizeof(geo_info_t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_geo_info_zeroed_on_init);
    RUN_TEST(test_public_ip_buf_has_space);
    RUN_TEST(test_region_fields_accept_chinese);
    RUN_TEST(test_struct_size_fits_stack);
    return UNITY_END();
}
