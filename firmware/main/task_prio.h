#pragma once

/*
 * FreeRTOS Task Priority Definitions
 * 
 * Display-First Architecture:
 *   display_task runs at HIGHEST priority (5) for 30fps I2C refresh.
 *   All other tasks run at lower priorities and must NOT block I2C/display.
 *   Communication between tasks via FreeRTOS Queues and Event Groups.
 */

// Display (CRITICAL — must not be blocked by any other task)
#define PRIO_DISPLAY    5   // 30fps I2C LED matrix refresh

// Rendering (computational, generates frame data for display)
#define PRIO_RENDER     4   // Background/foreground effect rendering

// Network & I/O (event-driven, lower priority)
#define PRIO_WIFI       3   // WiFi connection management + event handling
#define PRIO_MQTT       3   // MQTT message handling (send/receive/config sync)
#define PRIO_BUTTON     3   // Button GPIO interrupt → event detection

// Background services (one-shot or periodic, lowest priority)
#define PRIO_NETWORK    2   // IP geolocation, timezone lookup, NTP sync
#define PRIO_OTA        2   // Firmware download and verification
#define PRIO_THIN_HTTP  1   // Thin mode HTTP server (only when needed)

// Task stack sizes (in words, for ESP32-S3)
#define STACK_DISPLAY   8192    // I2C operations + framebuffer swap
#define STACK_RENDER    16384   // Complex rendering algorithms
#define STACK_WIFI      8192    // WiFi event handling
#define STACK_MQTT      12288   // MQTT + JSON parsing
#define STACK_BUTTON    4096    // Button debounce + event detection
#define STACK_NETWORK   10240   // HTTP client + JSON + NTP
#define STACK_OTA       12288   // HTTPS download + SHA256 verify
#define STACK_THIN_HTTP 8192    // HTTP server with SPIFFS
