#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/*
 * Double-Buffered Framebuffer for 48×16 LED Matrix
 * 
 * Architecture:
 *   - fb_front: currently being displayed (read by display_task)
 *   - fb_back:  currently being rendered (written by render_task)
 *   - fb_mutex: semaphore for atomic buffer swap
 * 
 * Workflow:
 *   1. render_task writes to fb_back (no lock needed)
 *   2. render_task calls fb_swap() → acquires mutex → swaps front/back → releases
 *   3. display_task reads from fb_front (no lock needed between swaps)
 */

#define FB_WIDTH   48
#define FB_HEIGHT  16

typedef struct {
    uint8_t pixels[FB_HEIGHT][FB_WIDTH];  // [row][col], 0=off, 255=max brightness
} framebuffer_t;

// Global framebuffers (defined in framebuffer.c)
extern framebuffer_t fb_front;   // Currently displayed
extern framebuffer_t fb_back;    // Currently being rendered
extern SemaphoreHandle_t fb_mutex; // Protects buffer swap

/**
 * Initialize framebuffers and mutex.
 * Call once at startup before any display/rendering tasks.
 */
void framebuffer_init(void);

/**
 * Get pointer to back buffer for rendering.
 * render_task writes to this buffer, then calls fb_swap().
 */
framebuffer_t* fb_get_back(void);

/**
 * Atomically swap front and back buffers.
 * Must be called by display_task (or render_task with mutex).
 * After swap, the old front becomes the new back (cleared at caller's discretion).
 */
void fb_swap(void);

/**
 * Clear entire framebuffer to 0 (all LEDs off).
 */
void fb_clear(framebuffer_t *fb);

/**
 * Set a single pixel. Clamps coordinates to valid range.
 */
void fb_set_pixel(framebuffer_t *fb, int x, int y, uint8_t brightness);
