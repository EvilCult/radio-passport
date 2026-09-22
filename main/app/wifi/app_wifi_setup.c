#include "app_wifi_setup.h"

#include "app_wifi_web.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static app_wifi_setup_status_callback_t status_callback;

static void app_wifi_setup_show_status(const char *status)
{
    if (status_callback == NULL) {
        return;
    }

    status_callback(status);
}

static void app_wifi_setup_connect_sta(void *user_data)
{
    (void)user_data;

    esp_err_t ret = esp_wifi_set_mode(
        WIFI_MODE_STA
    );

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("Failed to switch Wi-Fi mode");
        return;
    }

    ret = esp_wifi_connect();

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("Failed to connect to Wi-Fi");
        return;
    }

    app_wifi_setup_show_status("Connecting to Wi-Fi...");
}

static void app_wifi_setup_start_ap(void)
{
    esp_err_t ret = nvs_flash_init();

    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ret = nvs_flash_erase();

        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("NVS initialization failed");
        return;
    }

    ret = esp_netif_init();

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        app_wifi_setup_show_status("Network initialization failed");
        return;
    }

    ret = esp_event_loop_create_default();

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        app_wifi_setup_show_status("Event loop initialization failed");
        return;
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init_config =
        WIFI_INIT_CONFIG_DEFAULT();

    ret = esp_wifi_init(&wifi_init_config);

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        app_wifi_setup_show_status("Wi-Fi initialization failed");
        return;
    }

    char ssid[32];

    snprintf(
        ssid,
        sizeof(ssid),
        "radio-%05lu",
        (unsigned long)(esp_random() % 100000)
    );

    wifi_config_t ap_config = {
        .ap = {
            .ssid = {0},
            .ssid_len = 0,
            .channel = 1,
            .password = {0},
            .max_connection = 2,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    memcpy(
        ap_config.ap.ssid,
        ssid,
        strlen(ssid)
    );

    ap_config.ap.ssid_len = strlen(ssid);

    ret = esp_wifi_set_mode(
        WIFI_MODE_APSTA
    );

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("Failed to set Wi-Fi mode");
        return;
    }

    ret = esp_wifi_set_config(
        WIFI_IF_AP,
        &ap_config
    );

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("Failed to configure access point");
        return;
    }

    ret = esp_wifi_start();

    if (ret != ESP_OK) {
        app_wifi_setup_show_status("Failed to start access point");
        return;
    }

    char status[128];

    snprintf(
        status,
        sizeof(status),
        "Connect to Wi-Fi: %s\nOpen 192.168.4.1",
        ssid
    );

    app_wifi_setup_show_status(status);

    app_wifi_web_start(
        status_callback,
        app_wifi_setup_connect_sta
    );
}

void app_wifi_setup_start(
    app_wifi_setup_status_callback_t callback
)
{
    status_callback = callback;

    app_wifi_setup_start_ap();
}
