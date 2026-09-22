#pragma once

typedef void (*app_wifi_setup_status_callback_t)(
    const char *status
);

void app_wifi_setup_start(
    app_wifi_setup_status_callback_t callback
);
