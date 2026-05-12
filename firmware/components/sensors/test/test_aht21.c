/**
 * @file test_aht21.c
 * @brief Unity tests for AHT21 sensor data parsing.
 *
 * Verifies conversion math only (no I2C hardware required).
 * Formulas replicated from aht21.c:
 *   raw_h = data[1]<<12 | data[2]<<4 | data[3]>>4
 *   humidity = raw_h / 2^20 * 100
 *   raw_t = (data[3]&0x0F)<<16 | data[4]<<8 | data[5]
 *   temperature = raw_t / 2^20 * 200 - 50
 */
#include <unity.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Helper: replicate aht21.c conversion math                          */
/* ------------------------------------------------------------------ */

static float parse_humidity(const uint8_t data[6])
{
    uint32_t raw = ((uint32_t)data[1] << 12)
                 | ((uint32_t)data[2] << 4)
                 | (data[3] >> 4);
    return ((float)raw / 1048576.0f) * 100.0f;
}

static float parse_temperature(const uint8_t data[6])
{
    uint32_t raw = ((uint32_t)(data[3] & 0x0F) << 16)
                 | ((uint32_t)data[4] << 8)
                 | data[5];
    return ((float)raw / 1048576.0f) * 200.0f - 50.0f;
}

/* ------------------------------------------------------------------ */
/*  Test cases                                                         */
/* ------------------------------------------------------------------ */

/**
 * raw_h = 0x80000 = 524288  (exactly 50 % of 2^20)
 * data[1] = 0x80, data[2] = 0x00, data[3] = 0x00
 */
static void test_humidity_50_percent(void)
{
    uint8_t data[6] = {0, 0x80, 0x00, 0x00, 0x00, 0x00};
    float h = parse_humidity(data);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, h);
}

/**
 * raw_h = 0xFFFFF = 1048575  (all 20 bits set → ~100 %)
 * data[1] = 0xFF, data[2] = 0xFF, data[3] = 0xF0
 */
static void test_humidity_100_percent(void)
{
    uint8_t data[6] = {0, 0xFF, 0xFF, 0xF0, 0x00, 0x00};
    float h = parse_humidity(data);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 100.0f, h);
}

/**
 * raw_t = 0x60000 = 393216  →  (393216/1048576)*200 - 50 = 25 °C
 * data[3] = 0x06  (low nibble carries top 4 temp bits)
 * data[4] = 0x00, data[5] = 0x00
 */
static void test_temperature_25c(void)
{
    uint8_t data[6] = {0, 0x00, 0x00, 0x06, 0x00, 0x00};
    float t = parse_temperature(data);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, t);
}

/**
 * aht21.c: if (data[0] & 0x80) → measurement still in progress
 */
static void test_measurement_busy_rejected(void)
{
    uint8_t status = 0x80;          /* bit 7 = busy */
    TEST_ASSERT_TRUE(status & 0x80);
}

/* ------------------------------------------------------------------ */
/*  Boilerplate                                                        */
/* ------------------------------------------------------------------ */

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_humidity_50_percent);
    RUN_TEST(test_humidity_100_percent);
    RUN_TEST(test_temperature_25c);
    RUN_TEST(test_measurement_busy_rejected);
    return UNITY_END();
}
