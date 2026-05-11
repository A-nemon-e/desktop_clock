#pragma once
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// Background Types (7 total)
// ============================================================
typedef enum {
    BG_FIRE = 0,          // Doom Fire effect
    BG_CODE_RAIN,         // Matrix code rain
    BG_WATER_RIPPLE,      // 2D water ripple simulation
    BG_GAME_OF_LIFE,      // Conway's Game of Life
    BG_HOURGLASS,         // Sand hourglass animation
    BG_PONG_CLOCK,        // Pong game with clock overlay
    BG_WEATHER,           // Weather display (sun/cloud/rain/snow/wind)
    BG_COUNT              // Total background types (for validation)
} bg_type_t;

// ============================================================
// Foreground Types (5 total)
// ============================================================
typedef enum {
    FG_CLOCK_HMS = 0,         // "HH:MM:SS" — full clock
    FG_CLOCK_HM,              // "HH:MM" — hours and minutes only
    FG_DATE_WEEK_FULL,        // Line1: "yyyy mm dd"  Line2: "Dayname" (full)
    FG_DATE_WEEK_COMPACT,     // "mm dd DAY" (3-letter abbreviation)
    FG_TEMP_HUMID,            // "Hum：XX%\nTemp： XX.X C" (Chinese colon, space after Temp colon)
    FG_COUNT
} fg_type_t;

// ============================================================
// Font Sizes (4 total)
// ============================================================
typedef enum {
    FONT_3x5 = 0,    // Compact: 3px wide × 5px tall per character
    FONT_5x5,        // Normal: 5×5 pixels
    FONT_7x9,        // Large: 7×9 pixels, includes weekday abbreviations
    FONT_LONG,       // Extra tall/narrow: ~2-3px wide × 11-14px tall
                      // Perfect for clock digits using full 16px screen height
    FONT_COUNT
} font_size_t;

// ============================================================
// Foreground Configuration (per-foreground settings)
// ============================================================
typedef struct {
    fg_type_t   type;           // Which foreground to show
    font_size_t font;           // Font size (ignored for Pong/Weather backgrounds)
    uint16_t    duration_ms;    // How long this foreground displays (for multi-fg cards)
                                // 0 = no auto-switch between foregrounds within same card
} fg_slot_t;

#define MAX_FG_PER_CARD 5   // Maximum foregrounds in one card

// ============================================================
// Card Configuration (one "slide" in the display rotation)
// ============================================================
typedef struct {
    bg_type_t   bg;                        // Background effect
    fg_slot_t   fgs[MAX_FG_PER_CARD];      // Foreground layers (0-5)
    uint8_t     fg_count;                  // Actual number of foregrounds used (0-5)
    int8_t      relative_brightness;       // -128 to +127: foreground brightness offset
                                           // Negative = foreground darker than background
                                           // Positive = foreground brighter than background
                                           // Applied as: fg_brightness = base * (1 + rel/128)
    uint16_t    switch_interval;           // Seconds before auto-switch to next card
                                           // 0 = only switch on double-click (no auto)
} card_t;

#define MAX_CARDS 9   // Maximum cards per device

// ============================================================
// Weather Condition Codes
// ============================================================
typedef enum {
    WEATHER_SUNNY = 0,     // 晴
    WEATHER_CLOUDY,        // 多云
    WEATHER_RAIN,          // 雨
    WEATHER_SNOW,          // 雪
    WEATHER_OVERCAST,      // 阴
    WEATHER_WINDY,         // 大风 (derived from wind scale)
    WEATHER_UNKNOWN = 255  // Unknown/no data
} weather_condition_t;

// ============================================================
// Button Events
// ============================================================
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SINGLE_CLICK,       // Toggle screen on/off
    BTN_EVENT_DOUBLE_CLICK,       // Switch to next card
    BTN_EVENT_LONG_PRESS_START,   // Start brightness adjustment
    BTN_EVENT_LONG_PRESS_HOLD,    // Continue adjustment (every 200ms)
    BTN_EVENT_LONG_PRESS_RELEASE, // Stop adjustment, reverse direction next time
} button_event_t;

// ============================================================
// Device Info (reported to backend via MQTT)
// ============================================================
typedef struct {
    char mac[18];           // MAC address string "aa:bb:cc:dd:ee:ff"
    char hw_rev[8];         // Hardware revision "0.1" or "0.2"
    char fw_version[16];    // Firmware version "1.0.0"
    char public_ip[46];     // Public IP address
    char region[128];       // Province/City/District
    char timezone[64];      // IANA timezone "Asia/Shanghai"
} device_info_t;

// ============================================================
// Geo/Timezone Info (from IP geolocation pipeline)
// ============================================================
typedef struct {
    char public_ip[46];
    char country[64];
    char region[64];        // Province/State
    char city[64];          // City
    char district[64];      // District (China-specific)
    char timezone[64];      // IANA timezone string
    float latitude;
    float longitude;
} geo_info_t;
