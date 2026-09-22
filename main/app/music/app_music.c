#include "app_music.h"

#include "app_menu.h"
#include "app_page.h"
#include "app_music_player.h"
#include "bsp_button.h"
#include "bsp_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lvgl.h"

#include <stdbool.h>

#define APP_MUSIC_BG_COLOR 0xFF5951

#define APP_MUSIC_LONG_PRESS_MS 1000
#define APP_MUSIC_UPDATE_MS 20

#define APP_MUSIC_URL \
    "http://10.0.1.11:3000/api/transcode?url=https%3A%2F%2Ftk.wavpub.com%2FWPTK_e6Z7fPK6h9sS3b66.mp3"

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} app_music_button_event_t;

static QueueHandle_t app_music_button_queue;
static lv_timer_t *app_music_timer;

static bool ok_pressed;
static TickType_t ok_pressed_tick;

static void app_music_button_callback(
    bsp_btn_t button,
    bsp_btn_ev_t event,
    void *user
)
{
    (void)user;

    if (app_music_button_queue == NULL) {
        return;
    }

    app_music_button_event_t button_event = {
        .button = button,
        .event = event,
    };

    xQueueSend(
        app_music_button_queue,
        &button_event,
        0
    );
}

static void app_music_back_to_menu(void *user_data)
{
    (void)user_data;

    app_music_player_stop();

    app_music_deinit();
    app_menu_init();
}

static void app_music_process_buttons(lv_timer_t *timer)
{
    (void)timer;

    app_music_button_event_t event;

    while (xQueueReceive(
        app_music_button_queue,
        &event,
        0
    )) {
        if (event.button != BSP_BTN_OK) {
            continue;
        }

        if (event.event == BSP_BTN_PRESS) {
            ok_pressed = true;
            ok_pressed_tick = xTaskGetTickCount();
        }
    }

    if (!ok_pressed) {
        return;
    }

    TickType_t elapsed =
        xTaskGetTickCount() - ok_pressed_tick;

    if (pdTICKS_TO_MS(elapsed) >= APP_MUSIC_LONG_PRESS_MS) {
        ok_pressed = false;

        lv_async_call(
            app_music_back_to_menu,
            NULL
        );
    }
}

void app_music_init(void)
{
    app_music_button_queue = xQueueCreate(
        8,
        sizeof(app_music_button_event_t)
    );

    if (app_music_button_queue == NULL) {
        return;
    }

    bsp_button_init(
        app_music_button_callback,
        NULL
    );

    if (app_music_player_init() != ESP_OK) {
        vQueueDelete(app_music_button_queue);
        app_music_button_queue = NULL;
        return;
    }

    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    app_page_clear();

    lv_obj_set_style_bg_color(
        app_page_get(),
        lv_color_hex(APP_MUSIC_BG_COLOR),
        0
    );

    app_music_timer = lv_timer_create(
        app_music_process_buttons,
        APP_MUSIC_UPDATE_MS,
        NULL
    );

    bsp_lvgl_unlock();

    app_music_player_play(APP_MUSIC_URL);
}

void app_music_deinit(void)
{
    app_music_player_stop();

    if (bsp_lvgl_lock(1000)) {
        if (app_music_timer != NULL) {
            lv_timer_delete(app_music_timer);
            app_music_timer = NULL;
        }

        bsp_lvgl_unlock();
    }

    if (app_music_button_queue != NULL) {
        vQueueDelete(app_music_button_queue);
        app_music_button_queue = NULL;
    }

    ok_pressed = false;
}
