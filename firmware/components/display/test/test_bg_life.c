#include <unity.h>
#include <stdint.h>
static int count_neighbors(uint8_t g[16][48], int r, int c) {
    int n=0;for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){if(dr==0&&dc==0)continue;int nr=r+dr,nc=c+dc;if(nr>=0&&nr<16&&nc>=0&&nc<48)n+=g[nr][nc];}return n;
}
static void test_still_life_block(void) {
    uint8_t g[16][48]={0};g[7][23]=g[7][24]=g[8][23]=g[8][24]=1;
    for(int r=7;r<=8;r++)for(int c=23;c<=24;c++) {
        int n=count_neighbors(g,r,c);
        TEST_ASSERT_EQUAL_INT(3,n);
    }
}
static void test_death_by_isolation(void) {
    uint8_t g[16][48]={0};g[7][23]=1;
    int n=count_neighbors(g,7,23);
    TEST_ASSERT_EQUAL_INT(0,n);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_still_life_block);RUN_TEST(test_death_by_isolation);return UNITY_END();}
