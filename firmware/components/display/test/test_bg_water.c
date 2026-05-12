#include <unity.h>
#include <stdint.h>
static int16_t h[16][48];
static void test_damping_reduces(void) {
    for(int r=0;r<16;r++)for(int c=0;c<48;c++)h[r][c]=256;
    for(int r=0;r<16;r++)for(int c=0;c<48;c++)h[r][c]-=h[r][c]>>6;
    for(int r=0;r<16;r++)for(int c=0;c<48;c++)TEST_ASSERT(h[r][c]<=256);
}
static void test_ripple_spreads(void) {
    memset(h,0,sizeof(h));h[8][24]=512;
    for(int r=1;r<15;r++)for(int c=1;c<47;c++) {
        int32_t sum=h[r-1][c]+h[r+1][c]+h[r][c-1]+h[r][c+1];
        h[r][c]=(sum/2)-h[r][c];h[r][c]-=h[r][c]>>6;
    }
    int count=0;for(int r=6;r<10;r++)for(int c=22;c<26;c++)if(h[r][c]!=0)count++;
    TEST_ASSERT_GREATER_THAN(0,count);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_damping_reduces);RUN_TEST(test_ripple_spreads);return UNITY_END();}
