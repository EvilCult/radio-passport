#pragma once

#include "app_wifi_setup.h"

typedef void (*app_wifi_web_connect_callback_t)(
    void *user_data
);

void app_wifi_web_start(
    app_wifi_setup_status_callback_t status_callback,
    app_wifi_web_connect_callback_t connect_callback
);
