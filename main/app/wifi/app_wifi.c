#include "app_wifi.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define APP_WIFI_NVS_NAMESPACE "wifi"
#define APP_WIFI_NVS_SSID "ssid"
#define APP_WIFI_NVS_PASSWORD "password"

void app_wifi_init(void)
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
        return;
    }

    ret = esp_netif_init();

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        return;
    }

    ret = esp_event_loop_create_default();

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        return;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config =
        WIFI_INIT_CONFIG_DEFAULT();

    ret = esp_wifi_init(&wifi_init_config);

    if (
        ret != ESP_OK &&
        ret != ESP_ERR_INVALID_STATE
    ) {
        return;
    }

    nvs_handle_t nvs;

    ret = nvs_open(
        APP_WIFI_NVS_NAMESPACE,
        NVS_READONLY,
        &nvs
    );

    if (ret != ESP_OK) {
        return;
    }

    char ssid[33];
    char password[65];

    size_t ssid_len = sizeof(ssid);
    size_t password_len = sizeof(password);

    ret = nvs_get_str(
        nvs,
        APP_WIFI_NVS_SSID,
        ssid,
        &ssid_len
    );

    if (ret == ESP_OK) {
        ret = nvs_get_str(
            nvs,
            APP_WIFI_NVS_PASSWORD,
            password,
            &password_len
        );
    }

    nvs_close(nvs);

    if (ret != ESP_OK) {
        return;
    }

    wifi_config_t config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };

    memcpy(
        config.sta.ssid,
        ssid,
        sizeof(config.sta.ssid)
    );

    memcpy(
        config.sta.password,
        password,
        sizeof(config.sta.password)
    );

    ret = esp_wifi_set_mode(
        WIFI_MODE_STA
    );

    if (ret != ESP_OK) {
        return;
    }

    ret = esp_wifi_set_config(
        WIFI_IF_STA,
        &config
    );

    if (ret != ESP_OK) {
        return;
    }

    esp_wifi_start();
    esp_wifi_connect();
}
