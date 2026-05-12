#include <unity.h>
#include <stdbool.h>
static bool primary_fails=false;
static int try_primary(void){if(primary_fails)return 1;return 0;}
static int try_backup(void){return 0;}
static void test_backup_on_primary_fail(void){
    primary_fails=true;
    int r=try_primary();
    if(r!=0)r=try_backup();
    TEST_ASSERT_EQUAL_INT(0,r);
}
static void test_primary_succeeds(void){
    primary_fails=false;
    int r=try_primary();
    TEST_ASSERT_EQUAL_INT(0,r);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_backup_on_primary_fail);RUN_TEST(test_primary_succeeds);return UNITY_END();}
