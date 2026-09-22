#include "app_setting_wifi.h"

#include "app_page.h"
#include "app_setting.h"
#include "app_wifi_setup.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

#define APP_SETTING_WIFI_BG_COLOR 0x154868

#define APP_SETTING_WIFI_UPDATE_MS 20
#define APP_SETTING_WIFI_LONG_PRESS_MS 1000

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} app_setting_wifi_button_event_t;

static QueueHandle_t app_setting_wifi_button_queue;
static lv_timer_t *app_setting_wifi_timer;

static bool ok_pressed;
static TickType_t ok_pressed_tick;

static lv_obj_t *status_label;

static void app_setting_wifi_show_status(const char *status)
{
    if (status_label == NULL) {
        return;
    }

    lv_label_set_text(
        status_label,
        status
    );
}

static void app_setting_wifi_button_callback(
    bsp_btn_t button,
    bsp_btn_ev_t event,
    void *user
)
{
    (void)user;

    if (app_setting_wifi_button_queue == NULL) {
        return;
    }

    app_setting_wifi_button_event_t button_event = {
        .button = button,
        .event = event,
    };

    xQueueSend(
        app_setting_wifi_button_queue,
        &button_event,
        0
    );
}

static void app_setting_wifi_back_to_setting(void *user_data)
{
    (void)user_data;

    app_setting_wifi_deinit();
    app_setting_init();
}

static void app_setting_wifi_process_buttons(lv_timer_t *timer)
{
    (void)timer;

    app_setting_wifi_button_event_t event;

    while (
        xQueueReceive(
            app_setting_wifi_button_queue,
            &event,
            0
        )
    ) {
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

    if (
        pdTICKS_TO_MS(elapsed) >=
        APP_SETTING_WIFI_LONG_PRESS_MS
    ) {
        ok_pressed = false;

        lv_async_call(
            app_setting_wifi_back_to_setting,
            NULL
        );
    }
}

void app_setting_wifi_init(void)
{
    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    app_page_clear();

    lv_obj_set_style_bg_color(
        app_page_get(),
        lv_color_hex(APP_SETTING_WIFI_BG_COLOR),
        0
    );

    status_label = lv_label_create(
        app_page_get()
    );

    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(0xFFFFFF),
        0
    );

    lv_obj_center(status_label);

    app_setting_wifi_button_queue = xQueueCreate(
        8,
        sizeof(app_setting_wifi_button_event_t)
    );

    bsp_button_init(
        app_setting_wifi_button_callback,
        NULL
    );

    app_setting_wifi_timer = lv_timer_create(
        app_setting_wifi_process_buttons,
        APP_SETTING_WIFI_UPDATE_MS,
        NULL
    );

    bsp_lvgl_unlock();

    app_wifi_setup_start(
        app_setting_wifi_show_status
    );
}

void app_setting_wifi_deinit(void)
{
    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    if (app_setting_wifi_timer != NULL) {
        lv_timer_delete(
            app_setting_wifi_timer
        );

        app_setting_wifi_timer = NULL;
    }

    status_label = NULL;

    bsp_lvgl_unlock();

    if (app_setting_wifi_button_queue != NULL) {
        vQueueDelete(
            app_setting_wifi_button_queue
        );

        app_setting_wifi_button_queue = NULL;
    }

    ok_pressed = false;
}
