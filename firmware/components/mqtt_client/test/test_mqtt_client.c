#include <unity.h>
#include <stdbool.h>
static int fail_count=0;
static bool thin_mode=false;
static void check_fail(void){
    fail_count++;
    if(fail_count>=3)thin_mode=true;
}
static void test_3_fails_triggers_thin(void){
    fail_count=0;thin_mode=false;
    for(int i=0;i<3;i++)check_fail();
    TEST_ASSERT_TRUE(thin_mode);
}
static void test_2_fails_no_thin(void){
    fail_count=0;thin_mode=false;
    for(int i=0;i<2;i++)check_fail();
    TEST_ASSERT_FALSE(thin_mode);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_3_fails_triggers_thin);RUN_TEST(test_2_fails_no_thin);return UNITY_END();}
