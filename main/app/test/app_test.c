#include "app_test.h"

#include "app_menu.h"
#include "app_page.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

#define APP_TEST_BG_COLOR 0xFF5951

#define APP_TEST_LONG_PRESS_MS 1000
#define APP_TEST_UPDATE_MS 20

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} app_test_button_event_t;

static QueueHandle_t app_test_button_queue;
static lv_timer_t *app_test_timer;

static bool ok_pressed;
static TickType_t ok_pressed_tick;

static void app_test_button_callback(
    bsp_btn_t button,
    bsp_btn_ev_t event,
    void *user
)
{
    (void)user;

    if (app_test_button_queue == NULL) {
        return;
    }

    app_test_button_event_t button_event = {
        .button = button,
        .event = event,
    };

    xQueueSend(
        app_test_button_queue,
        &button_event,
        0
    );
}

static void app_test_back_to_menu(void *user_data)
{
    (void)user_data;

    app_test_deinit();
    app_menu_init();
}

static void app_test_process_buttons(lv_timer_t *timer)
{
    (void)timer;

    app_test_button_event_t event;

    while (xQueueReceive(app_test_button_queue, &event, 0)) {
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

    TickType_t elapsed = xTaskGetTickCount() - ok_pressed_tick;

    if (pdTICKS_TO_MS(elapsed) >= APP_TEST_LONG_PRESS_MS) {
        ok_pressed = false;

        lv_async_call(
            app_test_back_to_menu,
            NULL
        );
    }
}

void app_test_init(void)
{
    app_test_button_queue = xQueueCreate(
        8,
        sizeof(app_test_button_event_t)
    );

    bsp_button_init(
        app_test_button_callback,
        NULL
    );

    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    app_page_clear();

    lv_obj_set_style_bg_color(
        app_page_get(),
        lv_color_hex(APP_TEST_BG_COLOR),
        0
    );

    app_test_timer = lv_timer_create(
        app_test_process_buttons,
        APP_TEST_UPDATE_MS,
        NULL
    );

    bsp_lvgl_unlock();
}

void app_test_deinit(void)
{
    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    if (app_test_timer != NULL) {
        lv_timer_delete(app_test_timer);
        app_test_timer = NULL;
    }

    bsp_lvgl_unlock();

    if (app_test_button_queue != NULL) {
        vQueueDelete(app_test_button_queue);
        app_test_button_queue = NULL;
    }

    ok_pressed = false;
}
