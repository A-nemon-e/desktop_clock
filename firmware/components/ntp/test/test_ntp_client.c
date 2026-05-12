#include <unity.h>
#include <string.h>
#define NTP_PRIMARY "ntp.aliyun.com"
#define NTP_BACKUP "pool.ntp.org"
static void test_primary_is_aliyun(void){TEST_ASSERT_EQUAL_STRING("ntp.aliyun.com",NTP_PRIMARY);}
static void test_backup_is_pool(void){TEST_ASSERT_EQUAL_STRING("pool.ntp.org",NTP_BACKUP);}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_primary_is_aliyun);RUN_TEST(test_backup_is_pool);return UNITY_END();}
