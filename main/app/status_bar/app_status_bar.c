#include "app_status_bar.h"

#include "bsp_battery.h"
#include "esp_wifi.h"
#include "lvgl.h"

#define APP_STATUS_BAR_COLOR 0xF9A01B
#define APP_STATUS_BAR_HEIGHT 32

#define APP_STATUS_BAR_BATTERY_UPDATE_MS 60000
#define APP_STATUS_BAR_WIFI_UPDATE_MS 5000

#define APP_STATUS_BAR_WIFI_DOT_SIZE 12
#define APP_STATUS_BAR_WIFI_MARGIN_LEFT 6

#define APP_STATUS_BAR_WIFI_DISCONNECTED_COLOR 0x808080
#define APP_STATUS_BAR_WIFI_CONNECTING_COLOR 0x3498DB
#define APP_STATUS_BAR_WIFI_CONNECTED_COLOR 0x2ECC71

static lv_obj_t *battery;
static lv_timer_t *battery_timer;

static lv_obj_t *wifi_dot;
static lv_timer_t *wifi_timer;

static void app_status_bar_update_battery(void)
{
    int battery_soc = bsp_battery_soc();

    if (battery_soc < 0) {
        return;
    }

    lv_label_set_text_fmt(
        battery,
        "%d%%",
        battery_soc
    );

    lv_color_t battery_color =
        lv_color_hex(0xE5E6EA);

    if (battery_soc < 20) {
        battery_color =
            lv_color_hex(0xFF5951);
    }

    lv_obj_set_style_text_color(
        battery,
        battery_color,
        0
    );
}

static void app_status_bar_update_wifi(void)
{
    wifi_ap_record_t ap_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        app_status_bar_set_wifi_state(
            APP_STATUS_BAR_WIFI_CONNECTED
        );

        return;
    }

    wifi_mode_t mode;

    if (esp_wifi_get_mode(&mode) == ESP_OK &&
        mode == WIFI_MODE_STA) {
        app_status_bar_set_wifi_state(
            APP_STATUS_BAR_WIFI_CONNECTING
        );

        return;
    }

    app_status_bar_set_wifi_state(
        APP_STATUS_BAR_WIFI_DISCONNECTED
    );
}

static void app_status_bar_battery_timer(lv_timer_t *timer)
{
    (void)timer;

    app_status_bar_update_battery();
}

static void app_status_bar_wifi_timer(lv_timer_t *timer)
{
    (void)timer;

    app_status_bar_update_wifi();
}

void app_status_bar_set_wifi_state(
    app_status_bar_wifi_state_t state
)
{
    if (wifi_dot == NULL) {
        return;
    }

    lv_color_t color;

    switch (state) {
        case APP_STATUS_BAR_WIFI_CONNECTING:
            color = lv_color_hex(
                APP_STATUS_BAR_WIFI_CONNECTING_COLOR
            );
            break;

        case APP_STATUS_BAR_WIFI_CONNECTED:
            color = lv_color_hex(
                APP_STATUS_BAR_WIFI_CONNECTED_COLOR
            );
            break;

        case APP_STATUS_BAR_WIFI_DISCONNECTED:
        default:
            color = lv_color_hex(
                APP_STATUS_BAR_WIFI_DISCONNECTED_COLOR
            );
            break;
    }

    lv_obj_set_style_bg_color(
        wifi_dot,
        color,
        0
    );
}

void app_status_bar_init(void)
{
    if (battery != NULL) {
        return;
    }

    lv_obj_t *status_bar =
        lv_obj_create(lv_screen_active());

    lv_obj_set_size(
        status_bar,
        LV_PCT(100),
        APP_STATUS_BAR_HEIGHT
    );

    lv_obj_set_pos(
        status_bar,
        0,
        0
    );

    lv_obj_set_style_bg_color(
        status_bar,
        lv_color_hex(APP_STATUS_BAR_COLOR),
        0
    );

    lv_obj_set_style_border_width(
        status_bar,
        0,
        0
    );

    lv_obj_set_style_radius(
        status_bar,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        status_bar,
        0,
        0
    );

    wifi_dot = lv_obj_create(status_bar);

    lv_obj_set_size(
        wifi_dot,
        APP_STATUS_BAR_WIFI_DOT_SIZE,
        APP_STATUS_BAR_WIFI_DOT_SIZE
    );

    lv_obj_set_style_radius(
        wifi_dot,
        LV_RADIUS_CIRCLE,
        0
    );

    lv_obj_set_style_border_width(
        wifi_dot,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        wifi_dot,
        0,
        0
    );

    lv_obj_align(
        wifi_dot,
        LV_ALIGN_LEFT_MID,
        APP_STATUS_BAR_WIFI_MARGIN_LEFT,
        0
    );

    app_status_bar_set_wifi_state(
        APP_STATUS_BAR_WIFI_DISCONNECTED
    );

    battery = lv_label_create(status_bar);

    app_status_bar_update_battery();

    lv_obj_set_style_text_font(
        battery,
        &lv_font_montserrat_14,
        0
    );

    lv_obj_set_style_bg_opa(
        battery,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        battery,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        battery,
        0,
        0
    );

    lv_obj_align(
        battery,
        LV_ALIGN_RIGHT_MID,
        -6,
        0
    );

    battery_timer = lv_timer_create(
        app_status_bar_battery_timer,
        APP_STATUS_BAR_BATTERY_UPDATE_MS,
        NULL
    );

    wifi_timer = lv_timer_create(
        app_status_bar_wifi_timer,
        APP_STATUS_BAR_WIFI_UPDATE_MS,
        NULL
    );
}
