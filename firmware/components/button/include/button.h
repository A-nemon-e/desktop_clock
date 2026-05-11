#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "types.h"

/**
 * @file button.h
 * @brief GPIO interrupt-based button driver with debounce and event detection.
 *
 * Button IO38, active HIGH (3.3V when pressed), rising edge interrupt.
 * Detects: SINGLE_CLICK, DOUBLE_CLICK, LONG_PRESS_START, LONG_PRESS_HOLD,
 *          LONG_PRESS_RELEASE.
 */

/**
 * @brief Initialize the button GPIO interrupt and start the processing task.
 *
 * Configures PIN_BUTTON (GPIO_NUM_38) as input with pull-down.
 * Installs ISR service for any-edge interrupts.
 * Creates an internal ISR queue and a button processing task.
 *
 * @return QueueHandle_t for receiving button_event_t events. Never fails
 *         (asserts on critical allocation failure). Caller reads events
 *         via button_get_event() or directly via xQueueReceive().
 */
QueueHandle_t button_init(void);

/**
 * @brief Get the next button event (blocking with timeout).
 *
 * Convenience wrapper around xQueueReceive(). Returns BTN_EVENT_NONE
 * when the timeout expires without an event.
 *
 * @param queue        Queue handle returned by button_init().
 * @param timeout_ticks FreeRTOS tick timeout (0 = no wait, portMAX_DELAY = wait forever).
 * @return button_event_t Next event or BTN_EVENT_NONE on timeout.
 */
button_event_t button_get_event(QueueHandle_t queue, TickType_t timeout_ticks);
