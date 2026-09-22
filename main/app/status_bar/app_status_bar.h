#pragma once

typedef enum {
    APP_STATUS_BAR_WIFI_DISCONNECTED,
    APP_STATUS_BAR_WIFI_CONNECTING,
    APP_STATUS_BAR_WIFI_CONNECTED,
} app_status_bar_wifi_state_t;

void app_status_bar_init(void);

void app_status_bar_set_wifi_state(
    app_status_bar_wifi_state_t state
);
