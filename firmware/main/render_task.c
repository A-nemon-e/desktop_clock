#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "framebuffer.h"
#include "card_mgr.h"
#include "font_renderer.h"
#include "bg_fire.h"
#include "bg_code_rain.h"
#include "bg_water.h"
#include "bg_life.h"
#include "bg_hourglass.h"
#include "fg_render.h"
#include "task_prio.h"
#include "esp_log.h"

#define TAG "RENDER"

static void bg_life_init_once(void) {
    static bool done = false;
    if (!done) { bg_life_init(42); done = true; }
}

void render_task_func(void *pv) {
    bg_life_init_once();
    uint32_t frame = 0;

    while (1) {
        card_t *card = card_get_current();
        framebuffer_t *fb = fb_get_back();
        fb_clear(fb);

        switch (card->bg) {
        case BG_FIRE:
            bg_fire_update();
            bg_fire_render(fb);
            break;
        case BG_CODE_RAIN:
            bg_code_rain_update();
            bg_code_rain_render(fb, NULL, 0);
            break;
        case BG_WATER_RIPPLE:
            bg_water_update();
            bg_water_render(fb);
            break;
        case BG_GAME_OF_LIFE:
            bg_life_update();
            bg_life_render(fb);
            break;
        case BG_HOURGLASS:
            bg_hourglass_update();
            bg_hourglass_render(fb);
            break;
        default:
            break;
        }

        for (int i = 0; i < card->fg_count; i++) {
            fg_slot_t *fg = &card->fgs[i];
            uint8_t brightness = 220; // TODO: apply card->relative_brightness
            switch (fg->type) {
            case FG_CLOCK_HMS:
                fg_clock_hms(fb, 2, 2, fg->font, brightness, 12, 34, 56);
                break;
            case FG_CLOCK_HM:
                fg_clock_hm(fb, 2, 2, fg->font, brightness, 12, 34);
                break;
            default:
                break;
            }
        }

        fb_swap();
        vTaskDelay(pdMS_TO_TICKS(33));
        frame++;
    }
}
