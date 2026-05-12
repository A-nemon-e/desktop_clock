#include <unity.h>
static int ball_x=23,ball_y=7,ball_dx=1,ball_dy=1;
static void move_ball(void){ball_x+=ball_dx;ball_y+=ball_dy;if(ball_y<=0||ball_y>=15)ball_dy=-ball_dy;}
static void test_ball_moves_each_frame(void){
    int ox=ball_x,oy=ball_y;move_ball();
    TEST_ASSERT(ox!=ball_x||oy!=ball_y);
}
static void test_wall_bounce(void){
    ball_y=0;ball_dy=-1;move_ball();
    TEST_ASSERT_EQUAL_INT(1,ball_dy);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_ball_moves_each_frame);RUN_TEST(test_wall_bounce);return UNITY_END();}
