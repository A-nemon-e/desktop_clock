#include <unity.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {int x,y,w,h;} rect_t;
static bool is_masked(const rect_t *m, int n, int x, int y) {
    for(int i=0;i<n;i++) if(x>=m[i].x&&x<m[i].x+m[i].w&&y>=m[i].y&&y<m[i].y+m[i].h) return true;
    return false;
}
static void test_mask_blocks(void) {
    rect_t m={10,2,28,12};
    TEST_ASSERT_TRUE(is_masked(&m,1,15,5));
    TEST_ASSERT_FALSE(is_masked(&m,1,0,0));
}
static void test_no_mask_allows(void) {
    TEST_ASSERT_FALSE(is_masked(NULL,0,20,8));
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_mask_blocks);RUN_TEST(test_no_mask_allows);return UNITY_END();}
