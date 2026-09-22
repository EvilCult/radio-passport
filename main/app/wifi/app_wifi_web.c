#include "app_wifi_web.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "lvgl.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>

#define APP_WIFI_WEB_SCAN_MAX 20

#define APP_WIFI_WEB_NVS_NAMESPACE "wifi"
#define APP_WIFI_WEB_NVS_SSID "ssid"
#define APP_WIFI_WEB_NVS_PASSWORD "password"

static app_wifi_setup_status_callback_t status_callback;
static app_wifi_web_connect_callback_t connect_callback;

static httpd_handle_t http_server;

static void app_wifi_web_show_status(const char *status)
{
    if (status_callback == NULL) {
        return;
    }

    status_callback(status);
}

static void app_wifi_web_connect_sta(void *user_data)
{
    if (connect_callback == NULL) {
        return;
    }

    connect_callback(user_data);
}

static esp_err_t app_wifi_web_root_handler(
    httpd_req_t *req
)
{
    const char *html =
        "<!DOCTYPE html>"
        "<html lang=\"zh-CN\">"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
        "<title>Wi-Fi 设置</title>"
        "<style>"
        "*{box-sizing:border-box}"
        "body{"
        "margin:0;"
        "padding:24px;"
        "font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;"
        "background:#f5f5f5;"
        "color:#222"
        "}"
        ".container{"
        "max-width:480px;"
        "margin:0 auto;"
        "background:#fff;"
        "padding:24px;"
        "border-radius:16px;"
        "box-shadow:0 4px 20px rgba(0,0,0,.08)"
        "}"
        "h2{"
        "margin:0 0 24px;"
        "font-size:24px"
        "}"
        "label{"
        "display:block;"
        "margin:16px 0 8px;"
        "font-size:14px;"
        "color:#666"
        "}"
        "input{"
        "width:100%;"
        "height:46px;"
        "padding:0 12px;"
        "border:1px solid #ddd;"
        "border-radius:8px;"
        "font-size:16px;"
        "outline:none"
        "}"
        "input:focus{"
        "border-color:#1677ff"
        "}"
        "button{"
        "width:100%;"
        "height:46px;"
        "margin-top:16px;"
        "border:0;"
        "border-radius:8px;"
        "background:#1677ff;"
        "color:#fff;"
        "font-size:16px;"
        "cursor:pointer"
        "}"
        "button:active{"
        "opacity:.8"
        "}"
        ".secondary{"
        "background:#f0f0f0;"
        "color:#222"
        "}"
        "#status{"
        "margin-top:16px;"
        "font-size:14px;"
        "color:#666;"
        "white-space:pre-wrap;"
        "}"
        ".modal{"
        "display:none;"
        "position:fixed;"
        "inset:0;"
        "background:rgba(0,0,0,.45);"
        "padding:24px;"
        "overflow:auto"
        "}"
        ".modal-content{"
        "max-width:480px;"
        "margin:40px auto;"
        "background:#fff;"
        "border-radius:16px;"
        "padding:20px"
        "}"
        ".modal-title{"
        "font-size:20px;"
        "font-weight:600;"
        "margin-bottom:16px"
        "}"
        ".scan-list{"
        "display:flex;"
        "flex-direction:column;"
        "gap:8px"
        "}"
        ".ap{"
        "display:flex;"
        "justify-content:space-between;"
        "align-items:center;"
        "min-height:48px;"
        "padding:10px 12px;"
        "border:1px solid #eee;"
        "border-radius:8px;"
        "cursor:pointer"
        "}"
        ".ap:active{"
        "background:#f5f5f5"
        "}"
        ".ap-ssid{"
        "min-width:0;"
        "overflow:hidden;"
        "text-overflow:ellipsis;"
        "white-space:nowrap"
        "}"
        ".ap-rssi{"
        "margin-left:12px;"
        "color:#888;"
        "font-size:13px;"
        "white-space:nowrap"
        "}"
        ".loading{"
        "height:48px;"
        "border-radius:8px;"
        "background:linear-gradient("
        "90deg,#f0f0f0 25%,#e5e5e5 50%,#f0f0f0 75%"
        ");"
        "background-size:200% 100%;"
        "animation:loading 1.2s infinite"
        "}"
        "@keyframes loading{"
        "0%{background-position:200% 0}"
        "100%{background-position:-200% 0}"
        "}"
        ".empty{"
        "padding:24px;"
        "text-align:center;"
        "color:#999"
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<div class=\"container\">"
        "<h2>Wi-Fi 设置</h2>"

        "<label for=\"ssid\">Wi-Fi 名称</label>"
        "<input id=\"ssid\" type=\"text\" autocomplete=\"off\">"

        "<label for=\"password\">Wi-Fi 密码</label>"
        "<input id=\"password\" type=\"password\" autocomplete=\"off\">"

        "<button class=\"secondary\" onclick=\"scan()\">"
        "搜索附近"
        "</button>"

        "<button onclick=\"save()\">"
        "记住"
        "</button>"

        "<div id=\"status\"></div>"
        "</div>"

        "<div id=\"modal\" class=\"modal\" onclick=\"closeModal(event)\">"
        "<div class=\"modal-content\">"
        "<div class=\"modal-title\">附近的 Wi-Fi</div>"
        "<div id=\"scan-list\" class=\"scan-list\"></div>"
        "<button class=\"secondary\" onclick=\"closeScanModal()\">"
        "取消"
        "</button>"
        "</div>"
        "</div>"

        "<script>"
        "function closeModal(event){"
        "if(event.target.id==='modal'){"
        "closeScanModal();"
        "}"
        "}"

        "function closeScanModal(){"
        "document.getElementById('modal').style.display='none';"
        "}"

        "async function scan(){"
        "const modal=document.getElementById('modal');"
        "const list=document.getElementById('scan-list');"

        "modal.style.display='block';"
        "list.innerHTML='<div class=\"loading\"></div>';"

        "try{"
        "const response=await fetch('/api/wifi/scan');"

        "if(!response.ok){"
        "throw new Error('scan failed');"
        "}"

        "const result=await response.json();"

        "if(!result.data||result.data.length===0){"
        "list.innerHTML="
        "'<div class=\"empty\">没有找到 Wi-Fi</div>';"
        "return;"
        "}"

        "list.innerHTML='';"

        "result.data.forEach(ap=>{"
        "const item=document.createElement('div');"
        "item.className='ap';"

        "const ssid=document.createElement('span');"
        "ssid.className='ap-ssid';"
        "ssid.textContent=ap.ssid;"

        "const rssi=document.createElement('span');"
        "rssi.className='ap-rssi';"
        "rssi.textContent=ap.rssi+' dBm';"

        "item.appendChild(ssid);"
        "item.appendChild(rssi);"

        "item.onclick=()=>{"
        "document.getElementById('ssid').value=ap.ssid;"
        "closeScanModal();"
        "};"

        "list.appendChild(item);"
        "});"

        "}catch(error){"
        "list.innerHTML="
        "'<div class=\"empty\">搜索失败</div>';"
        "}"
        "}"

        "async function save(){"
        "const ssid=document.getElementById('ssid').value;"
        "const password=document.getElementById('password').value;"
        "const status=document.getElementById('status');"

        "if(!ssid){"
        "status.textContent='请输入 Wi-Fi 名称';"
        "return;"
        "}"

        "status.textContent='保存中...';"

        "try{"
        "const response=await fetch('/api/wifi/save',{"
        "method:'POST',"
        "headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({ssid:ssid,password:password})"
        "});"

        "const text=await response.text();"

        "if(!response.ok){"
        "status.textContent=text||'保存失败';"
        "return;"
        "}"

        "status.textContent=text;"
        "}catch(error){"
        "status.textContent='保存失败';"
        "}"
        "}"
        "</script>"
        "</body>"
        "</html>";

    httpd_resp_set_type(
        req,
        "text/html; charset=utf-8"
    );

    return httpd_resp_send(
        req,
        html,
        HTTPD_RESP_USE_STRLEN
    );
}

static esp_err_t app_wifi_web_scan_handler(
    httpd_req_t *req
)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(
        &scan_config,
        true
    );

    if (ret != ESP_OK) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Wi-Fi scan failed"
        );
    }

    uint16_t count = 0;

    ret = esp_wifi_scan_get_ap_num(
        &count
    );

    if (ret != ESP_OK) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Wi-Fi scan failed"
        );
    }

    if (count > APP_WIFI_WEB_SCAN_MAX) {
        count = APP_WIFI_WEB_SCAN_MAX;
    }

    wifi_ap_record_t records[
        APP_WIFI_WEB_SCAN_MAX
    ];

    memset(
        records,
        0,
        sizeof(records)
    );

    ret = esp_wifi_scan_get_ap_records(
        &count,
        records
    );

    if (ret != ESP_OK) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Wi-Fi scan failed"
        );
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Out of memory"
        );
    }

    cJSON *data = cJSON_CreateArray();

    if (data == NULL) {
        cJSON_Delete(root);

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Out of memory"
        );
    }

    cJSON_AddItemToObject(
        root,
        "data",
        data
    );

    for (uint16_t i = 0; i < count; i++) {
        if (records[i].ssid[0] == '\0') {
            continue;
        }

        cJSON *item = cJSON_CreateObject();

        if (item == NULL) {
            cJSON_Delete(root);

            return httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "Out of memory"
            );
        }

        cJSON_AddStringToObject(
            item,
            "ssid",
            (const char *)records[i].ssid
        );

        cJSON_AddNumberToObject(
            item,
            "rssi",
            records[i].rssi
        );

        cJSON_AddItemToArray(
            data,
            item
        );
    }

    char *json = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    if (json == NULL) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Out of memory"
        );
    }

    httpd_resp_set_type(
        req,
        "application/json"
    );

    esp_err_t response_ret = httpd_resp_send(
        req,
        json,
        HTTPD_RESP_USE_STRLEN
    );

    free(json);

    return response_ret;
}

static esp_err_t app_wifi_web_save_handler(
    httpd_req_t *req
)
{
    if (
        req->content_len <= 0 ||
        req->content_len > 1024
    ) {
        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid request"
        );
    }

    char *body = malloc(
        req->content_len + 1
    );

    if (body == NULL) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Out of memory"
        );
    }

    int received = httpd_req_recv(
        req,
        body,
        req->content_len
    );

    if (received <= 0) {
        free(body);

        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid request"
        );
    }

    body[received] = '\0';

    cJSON *json = cJSON_Parse(body);

    free(body);

    if (json == NULL) {
        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid JSON"
        );
    }

    cJSON *ssid_item = cJSON_GetObjectItem(
        json,
        "ssid"
    );

    cJSON *password_item = cJSON_GetObjectItem(
        json,
        "password"
    );

    if (
        !cJSON_IsString(ssid_item) ||
        !cJSON_IsString(password_item)
    ) {
        cJSON_Delete(json);

        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid Wi-Fi data"
        );
    }

    const char *ssid =
        ssid_item->valuestring;

    const char *password =
        password_item->valuestring;

    nvs_handle_t nvs;

    esp_err_t ret = nvs_open(
        APP_WIFI_WEB_NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs
    );

    if (ret == ESP_OK) {
        ret = nvs_set_str(
            nvs,
            APP_WIFI_WEB_NVS_SSID,
            ssid
        );
    }

    if (ret == ESP_OK) {
        ret = nvs_set_str(
            nvs,
            APP_WIFI_WEB_NVS_PASSWORD,
            password
        );
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

    if (ret == ESP_OK) {
        nvs_close(nvs);
    } else {
        nvs_close(nvs);

        cJSON_Delete(json);

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Save failed"
        );
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

    ret = esp_wifi_set_config(
        WIFI_IF_STA,
        &config
    );

    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Wi-Fi configuration failed"
        );
    }

    app_wifi_web_show_status(
        "已保存，正在连接 Wi-Fi"
    );

    /*
     * 必须先把 HTTP 响应发送出去。
     *
     * 如果这里直接切换 STA，
     * AP 会立即消失，浏览器可能收到保存失败。
     */
    esp_err_t response_ret = httpd_resp_send(
        req,
        "已保存，正在连接 Wi-Fi",
        HTTPD_RESP_USE_STRLEN
    );

    if (response_ret != ESP_OK) {
        return response_ret;
    }

    /*
     * HTTP 响应发送完成后，
     * 再异步切换到 STA。
     */
    lv_async_call(
        app_wifi_web_connect_sta,
        NULL
    );

    return ESP_OK;
}

static void app_wifi_web_start_http_server(void)
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;
    config.max_uri_handlers = 8;

    esp_err_t ret = httpd_start(
        &http_server,
        &config
    );

    if (ret != ESP_OK) {
        app_wifi_web_show_status(
            "HTTP Server 启动失败"
        );

        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = app_wifi_web_root_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(
        http_server,
        &root_uri
    );

    httpd_uri_t scan_uri = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = app_wifi_web_scan_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(
        http_server,
        &scan_uri
    );

    httpd_uri_t save_uri = {
        .uri = "/api/wifi/save",
        .method = HTTP_POST,
        .handler = app_wifi_web_save_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(
        http_server,
        &save_uri
    );
}

void app_wifi_web_start(
    app_wifi_setup_status_callback_t status_callback_arg,
    app_wifi_web_connect_callback_t connect_callback_arg
)
{
    status_callback = status_callback_arg;
    connect_callback = connect_callback_arg;

    app_wifi_web_start_http_server();
}
