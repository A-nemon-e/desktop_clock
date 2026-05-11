#include "button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pinmap.h"

static const char *TAG = "BTN";

// Timing constants (all in milliseconds)
#define DEBOUNCE_MS          50
#define DOUBLE_CLICK_MAX_MS 300
#define LONG_PRESS_MIN_MS   800
#define LONG_PRESS_HOLD_MS  200

// Size of the ISR-to-task raw edge queue
#define ISR_QUEUE_LEN   16
#define EVT_QUEUE_LEN   16

// ============================================================
// ISR — edge timestamp + level sent to task via queue
// ============================================================

typedef struct {
    int64_t timestamp_us;  // esp_timer time of the GPIO edge
    bool    level;         // GPIO level at edge (1 = rising, 0 = falling)
} button_isr_msg_t;

static QueueHandle_t s_isr_queue = NULL;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    button_isr_msg_t msg = {
        .timestamp_us = esp_timer_get_time(),
        .level = gpio_get_level(PIN_BUTTON),
    };
    xQueueSendFromISR(s_isr_queue, &msg, NULL);
}

// ============================================================
// Button processing task — debounce + event state machine
// ============================================================

typedef enum {
    ST_IDLE,
    ST_PRESSED,          // Debounced press confirmed
    ST_LONG_PRESS,       // Long press threshold reached, sending HOLD events
    ST_WAIT_DOUBLE,      // Short press released, waiting for second click or timeout
} btn_state_t;

static void button_task(void *arg)
{
    QueueHandle_t evt_queue = (QueueHandle_t)arg;
    button_isr_msg_t isr_msg;

    // Debounce tracking
    bool stable_level = gpio_get_level(PIN_BUTTON);   // Last confirmed level
    bool debouncing = false;
    int64_t debounce_start = 0;   // When current debounce period started

    // Event state machine
    btn_state_t state = ST_IDLE;
    int64_t press_time = 0;       // When press was confirmed (for long press detect)
    int64_t hold_last_time = 0;   // Last LONG_PRESS_HOLD event time
    int64_t release_time = 0;     // When short-press release was confirmed (double-click window)
    bool second_press = false;    // This press is the second of a double-click

    while (1) {
        // Block on ISR queue with 10 ms timeout
        // The ISR event is used as a wake-up signal; actual level is sampled below.
        xQueueReceive(s_isr_queue, &isr_msg, pdMS_TO_TICKS(10));

        int64_t now = esp_timer_get_time();
        bool level = gpio_get_level(PIN_BUTTON);

        // --------------------------------------------------
        // Debounce: wait DEBOUNCE_MS of consistent level
        // --------------------------------------------------
        if (level != stable_level) {
            if (!debouncing) {
                debouncing = true;
                debounce_start = now;
            } else if (now - debounce_start >= DEBOUNCE_MS * 1000) {
                stable_level = level;
                debouncing = false;
            }
        } else {
            debouncing = false;   // Cancel debounce when level returns to stable
        }

        // --------------------------------------------------
        // State machine (only when not debouncing)
        // --------------------------------------------------
        if (!debouncing) {
            switch (state) {

            case ST_IDLE:
                if (stable_level == 1) {
                    second_press = false;  // Reset flag from any previous double-click sequence
                    state = ST_PRESSED;
                    press_time = now;
                    hold_last_time = now;
                }
                break;

            case ST_PRESSED:
                if (stable_level == 0) {
                    // Release confirmed
                    release_time = now;
                    if (second_press) {
                        // Already sent DOUBLE_CLICK on second press; just reset
                        second_press = false;
                        state = ST_IDLE;
                    } else {
                        state = ST_WAIT_DOUBLE;
                    }
                } else if (now - press_time >= LONG_PRESS_MIN_MS * 1000) {
                    // Long press threshold reached
                    state = ST_LONG_PRESS;
                    hold_last_time = now;
                    button_event_t evt = BTN_EVENT_LONG_PRESS_START;
                    xQueueSend(evt_queue, &evt, 0);
                }
                break;

            case ST_LONG_PRESS:
                if (stable_level == 0) {
                    state = ST_IDLE;
                    button_event_t evt = BTN_EVENT_LONG_PRESS_RELEASE;
                    xQueueSend(evt_queue, &evt, 0);
                } else if (now - hold_last_time >= LONG_PRESS_HOLD_MS * 1000) {
                    hold_last_time = now;
                    button_event_t evt = BTN_EVENT_LONG_PRESS_HOLD;
                    xQueueSend(evt_queue, &evt, 0);
                }
                break;

            case ST_WAIT_DOUBLE:
                if (stable_level == 1) {
                    // Second press detected — this is a double-click
                    second_press = true;
                    state = ST_PRESSED;
                    press_time = now;
                    hold_last_time = now;
                    button_event_t evt = BTN_EVENT_DOUBLE_CLICK;
                    xQueueSend(evt_queue, &evt, 0);
                } else if (now - release_time >= DOUBLE_CLICK_MAX_MS * 1000) {
                    // Single-click timeout
                    state = ST_IDLE;
                    button_event_t evt = BTN_EVENT_SINGLE_CLICK;
                    xQueueSend(evt_queue, &evt, 0);
                }
                break;
            }
        }
    }
}

// ============================================================
// Public API
// ============================================================

QueueHandle_t button_init(void)
{
    // Configure button GPIO: input with pull-down, any-edge interrupt
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&btn_conf);

    // Create ISR queue (raw edges) and event queue (processed events)
    s_isr_queue = xQueueCreate(ISR_QUEUE_LEN, sizeof(button_isr_msg_t));
    QueueHandle_t evt_queue = xQueueCreate(EVT_QUEUE_LEN, sizeof(button_event_t));

    // Both queues must be created; if either fails, halt (system cannot function
    // without button input).
    configASSERT(s_isr_queue != NULL);
    configASSERT(evt_queue != NULL);

    // Install global ISR service and attach handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON, button_isr_handler, NULL);

    // Spawn dedicated processing task
    xTaskCreate(button_task, "btn_task", 3072, evt_queue, 10, NULL);

    ESP_LOGI(TAG, "Button initialized on GPIO %d", PIN_BUTTON);
    return evt_queue;
}

button_event_t button_get_event(QueueHandle_t queue, TickType_t timeout_ticks)
{
    button_event_t evt = BTN_EVENT_NONE;
    xQueueReceive(queue, &evt, timeout_ticks);
    return evt;
}
