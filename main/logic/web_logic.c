#include <cJSON.h>
#include <esp_log.h>

#include <device_config.h>
#include <eventbus.h>

#define TAG "UI"

esp_err_t
on_root(component_base_t* subscriber, event_t* event) {
    extern const unsigned char index_start[] asm("_binary_index_html_start");
    extern const unsigned char index_end[] asm("_binary_index_html_end");
    webserver_req_buffer_t* request = (webserver_req_buffer_t*)event->data;
    webserver_req_buffer_t response = {
        .req = request->req,
        .buffer = {.ptr = (char*)index_start, .size = index_end - index_start, .persistent = true},
    };
    httpd_resp_set_type(response.req, HTTPD_TYPE_TEXT);
    return webserver_enqueue_response(&response);
}

char* msg_success = "{\"ret\":\"success\"}";

esp_err_t
on_test(component_base_t* subscriber, event_t* event) {
    webserver_req_buffer_t* request = (webserver_req_buffer_t*)event->data;

    ESP_LOGI(TAG, "%s", request->buffer.ptr);

    cJSON* input = cJSON_Parse(request->buffer.ptr);
    int index = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(input, "index"));
    int r = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(input, "r"));
    int g = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(input, "g"));
    int b = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(input, "b"));
    cJSON_Delete(input);

    led_status_t st = {
        .type = LED_STATE_STEADY,
        .red = r,
        .green = g,
        .blue = b,
    };
    ws_led_set(index, &st, NULL);

    webserver_req_buffer_t response = {
        .req = request->req,
        .buffer = {.ptr = msg_success, .size = strlen(msg_success), .persistent = true},
    };
    httpd_resp_set_type(response.req, HTTPD_TYPE_TEXT);
    return webserver_enqueue_response(&response);
}

webserver_uri_t URIS[] = {
    {
        .uri = "/",
        .method = HTTP_GET,
        .event_id = EVT_WEBSERVER_UI_ON_ROOT,
        .handler = on_root,
    },
    {
        .uri = "/test",
        .method = HTTP_POST,
        .event_id = EVT_WEBSERVER_UI_ON_TEST,
        .handler = on_test,
    },
};

esp_err_t
web_activity_handler(component_base_t* subscriber, event_t* event) {
    esp_err_t err = ESP_OK;
    if (event->id == EVT_WEBSERVER_INACTIVE) {
        ESP_LOGW(TAG, "EVT_WEBSERVER_INACTIVE");
        err = webserver_stop();
    } else if (event->id == EVT_WEBSERVER_ON) {
        ESP_LOGW(TAG, "EVT_WEBSERVER_ON");
    } else if (event->id == EVT_WEBSERVER_OFF) {
        ESP_LOGW(TAG, "EVT_WEBSERVER_OFF");
    }
    return err;
}

esp_err_t
wifi_middelware(component_base_t* subscriber, event_t* event) {
    esp_err_t err = ESP_OK;
    webserver_component_t* webserver = (webserver_component_t*)subscriber;
    wifi_event_data_t evt_data = (wifi_event_data_t)event->data;

    if (event->id == EVT_WIFI_STA_CONNECTION) {
        if (evt_data.success) {
            if (!webserver->is_started) {
                err = webserver_start_http();
            }
        } else {
            ESP_LOGW(TAG, "Wifi off, web is still alive");
        }
    }
    return err;
}

esp_err_t
web_logic() {
    webserver_component_config_t web_cfg = {
        .max_open_sockets = 7,
        .uris = URIS,
        .uris_size = sizeof(URIS) / sizeof(URIS[0]),
        // .inactive_shutdown_ms = 15 * 1000,
        .inactive_shutdown_ms = 0,
    };
    webserver_component_t* webserver = webserver_create(&web_cfg);
    device_module_add_middleware(&webserver->base, WEBSERVER_MODULE, web_activity_handler);

    wifi_component_config_t wifi_cfg = {
        .mode = WIFI_MODE_STA,
        .reconnect_attempts = 3,
        .reconnect_interval_ms = 7000,
    };
    wifi_network_create(&wifi_cfg);

    return device_module_add_middleware(&webserver->base, WIFI_MODULE, wifi_middelware);
}