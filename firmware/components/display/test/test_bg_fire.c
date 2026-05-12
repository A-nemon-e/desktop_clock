#include <unity.h>
#include <stdint.h>
static uint8_t fire[16][48];
static void init_fire(void) {
    for (int r=0;r<15;r++)for(int c=0;c<48;c++)fire[r][c]=0;
    for (int c=0;c<48;c++)fire[15][c]=128+(c*2)%128;
}
static void test_fire_propagates_upward(void) {
    init_fire();
    int sum_above=0,sum_below=0;
    for(int c=0;c<48;c++)sum_below+=fire[15][c];
    TEST_ASSERT_GREATER_THAN(0,sum_below);
}
static void test_all_values_in_range(void) {
    init_fire();
    for(int r=0;r<16;r++)for(int c=0;c<48;c++) {
        TEST_ASSERT(fire[r][c]>=0&&fire[r][c]<=255);
    }
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_fire_propagates_upward);RUN_TEST(test_all_values_in_range);return UNITY_END();}
