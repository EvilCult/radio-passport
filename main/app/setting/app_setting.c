#include "app_setting.h"

#include "app_page.h"
#include "app_setting_wifi.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

#define APP_SETTING_BG_COLOR 0x154868

#define APP_SETTING_BUTTON_WIDTH 200
#define APP_SETTING_BUTTON_HEIGHT 48
#define APP_SETTING_BUTTON_RADIUS 24

#define APP_SETTING_BUTTON_COLOR 0x15B9C7
#define APP_SETTING_BUTTON_SELECTED_COLOR 0xF9A01B
#define APP_SETTING_TEXT_COLOR 0x154868

#define APP_SETTING_UPDATE_MS 20
#define APP_SETTING_LONG_PRESS_MS 1000

typedef struct {
    const char *title;
    void (*action)(void);
} app_setting_item_t;

static const app_setting_item_t setting_items[] = {
    {
        .title = "Wi-Fi",
        .action = app_setting_wifi_init,
    },
};

#define APP_SETTING_ITEM_COUNT \
    (sizeof(setting_items) / sizeof(setting_items[0]))

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} app_setting_button_event_t;

static QueueHandle_t app_setting_button_queue;
static lv_timer_t *app_setting_timer;

static lv_obj_t *buttons[APP_SETTING_ITEM_COUNT];

static int selected_button = 0;

static bool ok_pressed;
static TickType_t ok_pressed_tick;

static void app_setting_button_callback(
    bsp_btn_t button,
    bsp_btn_ev_t event,
    void *user
)
{
    (void)user;

    if (app_setting_button_queue == NULL) {
        return;
    }

    app_setting_button_event_t button_event = {
        .button = button,
        .event = event,
    };

    xQueueSend(
        app_setting_button_queue,
        &button_event,
        0
    );
}

static void app_setting_update_selection(void)
{
    for (int i = 0; i < APP_SETTING_ITEM_COUNT; i++) {
        lv_obj_set_style_bg_color(
            buttons[i],
            lv_color_hex(
                i == selected_button
                    ? APP_SETTING_BUTTON_SELECTED_COLOR
                    : APP_SETTING_BUTTON_COLOR
            ),
            0
        );
    }
}

static void app_setting_process_buttons(lv_timer_t *timer)
{
    (void)timer;

    app_setting_button_event_t event;

    while (
        xQueueReceive(
            app_setting_button_queue,
            &event,
            0
        )
    ) {
        if (event.event != BSP_BTN_PRESS) {
            continue;
        }

        if (event.button == BSP_BTN_UP) {
            selected_button--;

            if (selected_button < 0) {
                selected_button = APP_SETTING_ITEM_COUNT - 1;
            }

            app_setting_update_selection();
        } else if (event.button == BSP_BTN_DOWN) {
            selected_button++;

            if (selected_button >= APP_SETTING_ITEM_COUNT) {
                selected_button = 0;
            }

            app_setting_update_selection();
        } else if (event.button == BSP_BTN_OK) {
            ok_pressed = true;
            ok_pressed_tick = xTaskGetTickCount();
        }
    }

    if (!ok_pressed) {
        return;
    }

    TickType_t elapsed =
        xTaskGetTickCount() - ok_pressed_tick;

    if (
        pdTICKS_TO_MS(elapsed) >=
        APP_SETTING_LONG_PRESS_MS
    ) {
        ok_pressed = false;

        lv_async_call(
            (lv_async_cb_t)setting_items[selected_button].action,
            NULL
        );
    }
}

static void app_setting_create_buttons(void)
{
    lv_obj_t *container = lv_obj_create(app_page_get());

    lv_obj_set_size(
        container,
        LV_PCT(100),
        LV_PCT(100)
    );

    lv_obj_set_pos(container, 0, 0);

    lv_obj_set_style_bg_opa(
        container,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        container,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        container,
        0,
        0
    );

    for (int i = 0; i < APP_SETTING_ITEM_COUNT; i++) {
        buttons[i] = lv_button_create(container);

        lv_obj_set_size(
            buttons[i],
            APP_SETTING_BUTTON_WIDTH,
            APP_SETTING_BUTTON_HEIGHT
        );

        lv_obj_set_style_radius(
            buttons[i],
            APP_SETTING_BUTTON_RADIUS,
            0
        );

        int offset =
            i * 64 -
            (APP_SETTING_ITEM_COUNT - 1) * 32;

        lv_obj_align(
            buttons[i],
            LV_ALIGN_CENTER,
            0,
            offset
        );

        lv_obj_t *label = lv_label_create(buttons[i]);

        lv_label_set_text(
            label,
            setting_items[i].title
        );

        lv_obj_set_style_text_color(
            label,
            lv_color_hex(APP_SETTING_TEXT_COLOR),
            0
        );

        lv_obj_center(label);
    }

    app_setting_update_selection();
}

void app_setting_init(void)
{
    app_setting_button_queue = xQueueCreate(
        8,
        sizeof(app_setting_button_event_t)
    );

    bsp_button_init(
        app_setting_button_callback,
        NULL
    );

    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    app_page_clear();

    lv_obj_set_style_bg_color(
        app_page_get(),
        lv_color_hex(APP_SETTING_BG_COLOR),
        0
    );

    app_setting_create_buttons();

    app_setting_timer = lv_timer_create(
        app_setting_process_buttons,
        APP_SETTING_UPDATE_MS,
        NULL
    );

    bsp_lvgl_unlock();
}

void app_setting_deinit(void)
{
    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    if (app_setting_timer != NULL) {
        lv_timer_delete(app_setting_timer);
        app_setting_timer = NULL;
    }

    bsp_lvgl_unlock();

    if (app_setting_button_queue != NULL) {
        vQueueDelete(app_setting_button_queue);
        app_setting_button_queue = NULL;
    }

    ok_pressed = false;
}
