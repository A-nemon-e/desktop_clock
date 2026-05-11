#pragma once
#include "driver/gpio.h"

// I2C Bus
#define PIN_I2C_SCL     GPIO_NUM_1
#define PIN_I2C_SDA     GPIO_NUM_2

// IS31FL3729 Shutdown (all 6 in parallel)
#define PIN_SDB         GPIO_NUM_8

// Status LED
#define PIN_LED         GPIO_NUM_4

// User Button (active high, pull-down)
#define PIN_BUTTON      GPIO_NUM_38

// I2S Audio (Phase 2)
#define PIN_I2S_WS      GPIO_NUM_15
#define PIN_I2S_CLK     GPIO_NUM_16
#define PIN_I2S_MICDATA GPIO_NUM_18

// USB (native, no GPIO config needed)
// IO19 = D-, IO20 = D+ (OTG, not used as GPIO)

// UART0 (debug)
// TXD0 = GPIO_NUM_43, RXD0 = GPIO_NUM_44 (ESP32-S3 native)
