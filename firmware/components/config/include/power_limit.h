#pragma once

/*
 * Power Budget for 48×16 White LED Matrix
 * ==========================================
 *
 * LED Matrix: 768 white LEDs (48 cols × 16 rows)
 * Per LED forward voltage: ~3.0-3.4V (white LED)
 * Per LED max current: 20mA
 * Theoretical max: 768 × 20mA × 3.3V = ~50.7W
 *
 * IS31FL3729 Global Current Control (GCC):
 * - Register 0xA1, range 0x00-0x7F (0-127)
 * - GCC scales ALL PWM outputs: I_led = I_max × (GCC / 127)
 * - PWM register per LED: 8-bit (0-255) fine control
 *
 * USB Power (Type-C 5V): typical 5V/2A = 10W max
 * ESP32-S3 consumption: ~0.5W
 * Available for LEDs: ~9.5W
 *
 * At full white (all LEDs on, PWM=255):
 * - GCC=127: I ≈ 15A theoretical → WAY over USB limit
 * - Need GCC to limit total current to ~2A @ 5V
 * - With IS31FL3729 efficiency ~85%: ~1.7A @ 3.3V for LEDs
 * - Per LED: 1.7A / 768 = ~2.2mA
 * - Required GCC: 127 × (2.2mA / 20mA) = 127 × 0.11 ≈ 14
 *
 * SAFE LIMITS (to be MEASURED and VERIFIED):
 * - MAX_GCC_VALUE = 20 (default safe, ~3.1mA/LED, ~2.4A total)
 * - MEASURED_GCC_MAX = TBD (set after actual measurement)
 */

#define MAX_GCC_VALUE_DEFAULT  20   // Safe default: limits total current to ~2.4A
#define MAX_GCC_VALUE_HARD     40   // Hard limit: ~4.8A — DO NOT EXCEED without measurement
#define GCC_MIN_VALUE           5   // Minimum visible brightness

// To be set after hardware measurement:
// #define MAX_GCC_VALUE  <measured_safe_value>
