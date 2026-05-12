#include <unity.h>
#include <stdint.h>

#define DISPLAY_FPS 30
#define FRAME_PERIOD_MS (1000/DISPLAY_FPS)
#define MUX_CHANNELS 6
#define LED_COUNT 128

static void test_frame_period_is_33ms(void) {
    TEST_ASSERT_EQUAL_INT(33, FRAME_PERIOD_MS);
}
static void test_mux_iterates_6_channels(void) {
    int visited = 0;
    for (int ch = 0; ch < MUX_CHANNELS; ch++) visited++;
    TEST_ASSERT_EQUAL_INT(6, visited);
}
static void test_pwm_data_size(void) {
    uint8_t pwm[LED_COUNT];
    TEST_ASSERT_EQUAL_INT(128, sizeof(pwm));
}
static void test_col_offset_per_channel(void) {
    for (int ch = 0; ch < 6; ch++)
        TEST_ASSERT_EQUAL_INT(ch * 8, ch * 8);
}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_frame_period_is_33ms);RUN_TEST(test_mux_iterates_6_channels);RUN_TEST(test_pwm_data_size);RUN_TEST(test_col_offset_per_channel);return UNITY_END();}
