#include <config_storage.h>
#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>
#include <wifi_network.h>

#include <web_logic.h>

#define TAG "UI"

esp_err_t
on_root(webserver_action_t* action) {
    extern const unsigned char index_start[] asm("_binary_index_html_start");
    extern const unsigned char index_end[] asm("_binary_index_html_end");

    webserver_action_t response = {
        .req = action->req,
        .buffer = (char*)index_start,
        .buffer_size = index_end - index_start,
        .need_free = false,
    };
    httpd_resp_set_type(response.req, HTTPD_TYPE_TEXT);
    return webserver_enqueue_response(&response);
}

webserver_uri_t URIS[] = {
    {
        .uri = "/",
        .method = HTTP_GET,
        .handler = on_root, // EVT_WEBSERVER_UI_ON_ROOT
    },
};

esp_err_t
ui_handler(module_base* self, event_t* event) {
    esp_err_t err = ESP_OK;
    if (event->issuer->id == self->id) { // Only for self
        if (event->id >= EVT_WEBSERVER_USER_URI) {
            webserver_action_t* action = (webserver_action_t*)event->payload.data.ptr;
            err = URIS[event->id - EVT_WEBSERVER_USER_URI].handler(action);
        } else if (event->id == EVT_WEBSERVER_INACTIVE) {
            ESP_LOGW(TAG, "EVT_WEBSERVER_INACTIVE");
            err = webserver_stop(self);
        } else if (event->id == EVT_WEBSERVER_ONOFF) {
            if (event->payload.data.b) {
                ESP_LOGW(TAG, "EVT_WEBSERVER_ON");
            } else {
                ESP_LOGW(TAG, "EVT_WEBSERVER_OFF");
            }
        }
    }
    return err;
}

esp_err_t
wifi_handler(module_base* self, event_t* event) {
    esp_err_t err = ESP_OK;
    if (event->id == EVT_WIFI_STA_CONNECTION) {
        module_base* web = eventbus_module_get(MODULE_WEB_UI);
        if (event->payload.data.b) {
            if (!is_webserver_started(web)) {
                err = webserver_start_http(web, URIS, sizeof(URIS) / sizeof(URIS[0]));
            }
        } else {
            ESP_LOGW(TAG, "Wifi off, web is still alive");
        }
    }
    return err;
}

esp_err_t
web_logic() {
    webserver_config_t config = {
        .max_open_sockets = 7,
        .event_handler = ui_handler,
        // .inactive_shutdown_ms = 15 * 1000,
        .inactive_shutdown_ms = 0,
    };
    webserver_create(MODULE_WEB_UI, &config, sizeof(URIS) / sizeof(URIS[0]));

    wifi_network_config_t wifi_con = {
        .mode = WIFI_MODE_STA,
        .reconnect_attempts = 3,
        .reconnect_interval_ms = 7000,
        .event_handler = wifi_handler,
    };
    module_base* wifi = wifi_network_create(MODULE_WIFI_NET, &wifi_con);

    return eventbus_module_subscribe(wifi, wifi->id, EVT_WIFI_STA_CONNECTION);
}