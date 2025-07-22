#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>
#include <web_ui.h>
#include <wifi_network.h>

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
    } else if (event->issuer->id == MODULE_WIFI_NET) {
        if (event->id == EVT_WIFI_STA_CONNECTION) {
            err = webserver_start_http(self, URIS, sizeof(URIS) / sizeof(URIS[0]));
        }
    }
    return err;
}

void
web_ui_create(int id) {
    webserver_config_t config = {
        .max_open_sockets = 7,
        .event_handler = ui_handler,
        // .inactive_shutdown_ms = 15 * 1000,
        .inactive_shutdown_ms = 0,
    };
    module_base* WEB = webserver_create(id, &config, sizeof(URIS) / sizeof(URIS[0]));
    module_subscribe(WEB, MODULE_WIFI_NET, EVT_WIFI_STA_CONNECTION);
}