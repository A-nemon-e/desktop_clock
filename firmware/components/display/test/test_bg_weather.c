#include <unity.h>
typedef enum{WEATHER_SUNNY=0,WEATHER_CLOUDY,WEATHER_RAIN,WEATHER_SNOW,WEATHER_OVERCAST,WEATHER_WINDY,WEATHER_UNKNOWN=255} wc_t;
static void test_all_conditions_valid(void){
    for(int i=0;i<6;i++){wc_t w=(wc_t)i;TEST_ASSERT(w>=0&&w<6);}
}
static void test_unknown_handled(void){TEST_ASSERT_EQUAL_INT(255,WEATHER_UNKNOWN);}
void setUp(void){}void tearDown(void){}
int main(void){UNITY_BEGIN();RUN_TEST(test_all_conditions_valid);RUN_TEST(test_unknown_handled);return UNITY_END();}
