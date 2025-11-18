#include <esp_log.h>
// #include "mdns.h"
#include <esp_timer.h>

#include "eventbus.h"
#include "webserver.h"

#define SEND_CHANK_SIZE                 40000 // RAM buffer for sending data in bytes (!caution!)
#define MAX_RECV_CONTENT_SIZE           8000

#define HTTP_413_CONTENT_TOO_LARGE      "413 Content Too Large"
#define HTTP_413_MSG                    "Maximum allowed size reached"
#define HTTP_415_UNSUPPORTED_MEDIA_TYPE "415 Unsupported Media Type"
#define HTTP_415_MSG                    "Unable to parse the request"

#define TAG                             "WEB"

typedef struct {
    module_base module;
    // char* hostname;
    httpd_handle_t server;
    webserver_config_t server_config;
    httpd_ssl_config_t https_config;
    esp_timer_handle_t shutdown_timer;
    bool is_started;
} webserver_t;

static char*
_read_payload_raw(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;
    if (total_len > MAX_RECV_CONTENT_SIZE) {
        httpd_resp_send_custom_err(req, HTTP_413_CONTENT_TOO_LARGE, HTTP_413_MSG);
        return NULL;
    }
    char* buffer = calloc(total_len + 1, sizeof(char));
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buffer + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
            free(buffer);
            return NULL;
        }
        cur_len += received;
    }
    ESP_LOGD(TAG, "post= %s", buffer);
    return buffer;
}

static esp_err_t
_send_chunked(httpd_req_t* req, const char* buf, const size_t size) {
    size_t sended = 0;
    size_t to_send = 0;
    do {
        size_t len = size - sended;
        to_send = len > SEND_CHANK_SIZE ? SEND_CHANK_SIZE : len;
        if (httpd_resp_send_chunk(req, buf + sended, to_send) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed!");
            httpd_resp_sendstr_chunk(req, NULL); // Aborting...
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
        sended += to_send;
        ESP_LOGD(TAG, "Chunk sended!");
    } while (sended < size);
    return httpd_resp_sendstr_chunk(req, NULL);
}

static void
_shutdown_inactivity_timer_callback(void* arg) {
    webserver_t* webserver = (webserver_t*)arg;
    event_t evt = {
        .id = EVT_WEBSERVER_INACTIVE,
        .issuer = &webserver->module,
        .payload = {.free_fcn = NULL},
    };
    eventbus_post_event(&evt);
    esp_timer_start_once(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
}

static void
_free_action(void* data_action) {
    webserver_action_t* action = (webserver_action_t*)data_action;
    if (action->need_free) {
        free(action->buffer);
    }
    free(action);
}

static esp_err_t
base_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "request:'%s'", req->uri);

    webserver_t* webserver = httpd_get_global_user_ctx(req->handle);

    if (webserver->server_config.inactive_shutdown_ms > 0) {
        esp_err_t err =
            esp_timer_restart(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "inactivity timer update err=%d(%s)", err, esp_err_to_name(err));
        }
        ESP_LOGD(TAG, "inactivity updated");
    }

    webserver_action_t* request = calloc(1, sizeof(webserver_action_t));
    if (httpd_req_async_handler_begin(req, &request->req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        free(request);
        return ESP_FAIL;
    }
    if (req->method != HTTP_GET) {
        request->buffer = _read_payload_raw(req);
        if (request->buffer == NULL) { // error codes are handled while read
            free(request);
            return ESP_FAIL;
        }
        request->buffer_size = strlen(request->buffer);
    }
    event_t evt = {
        .id = (int)request->req->user_ctx,
        .issuer = (module_base*)webserver,
        .payload = {.data.ptr = (void*)request, .size = sizeof(webserver_action_t), .free_fcn = _free_action},
    };
    return eventbus_post_event(&evt);
}

static void
_async_send_handler(void* arg) {
    webserver_action_t* async_response = (webserver_action_t*)arg;
    if (_send_chunked(async_response->req, async_response->buffer, async_response->buffer_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send to '%s'", async_response->req->uri);
    }
    if (httpd_req_async_handler_complete(async_response->req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to complete async req");
    }
    _free_action(async_response);
}

static void
_when_httpd_stop(void* global_user_ctx) {
    webserver_t* webserver = (webserver_t*)global_user_ctx;
    esp_timer_stop(webserver->shutdown_timer);
}

esp_err_t
webserver_enqueue_response(webserver_action_t* response) {
    webserver_action_t* async_response = calloc(1, sizeof(webserver_action_t));
    memcpy(async_response, response, sizeof(webserver_action_t));
    return httpd_queue_work(response->req->handle, _async_send_handler, async_response);
}

esp_err_t
webserver_start_http(module_base* self, webserver_uri_t* uri_array, size_t uri_amount) {
    esp_err_t err = ESP_FAIL;
    webserver_t* webserver = (webserver_t*)self;

    if (webserver->server == NULL) {
        webserver->https_config.httpd.server_port = 80;
        webserver->https_config.httpd.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT;
        webserver->https_config.httpd.max_uri_handlers = uri_amount;

        err = httpd_start(&webserver->server, &webserver->https_config.httpd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed start http server err=%d(%s)", err, esp_err_to_name(err));
        } else {
            for (int i = 0; i < uri_amount; i++) {
                httpd_uri_t uri = {
                    .uri = uri_array[i].uri,
                    .method = uri_array[i].method,
                    .handler = base_handler,
                    .user_ctx = (void*)(i + EVT_WEBSERVER_USER_URI), // (!) should be EVENT_ID
                };
                ESP_LOGI(TAG, "register uri='%s'", uri.uri);
                err = httpd_register_uri_handler(webserver->server, &uri);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register uri='%s'", uri.uri);
                    break;
                }
            }
            if (err == ESP_OK) {
                webserver->is_started = true;
                event_t evt = {
                    .id = EVT_WEBSERVER_ONOFF,
                    .issuer = self,
                    .payload = {.data.b = true, .free_fcn = NULL},
                };
                eventbus_post_event(&evt);
            }
            if (webserver->server_config.inactive_shutdown_ms) {
                esp_timer_start_once(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
            }
        }
    }
    return err;
}

bool
is_webserver_started(module_base* self) {
    webserver_t* webserver = (webserver_t*)self;
    return webserver->is_started;
}

esp_err_t
webserver_stop(module_base* self) {
    webserver_t* webserver = (webserver_t*)self;
    esp_err_t err = ESP_FAIL;
    if (webserver->server != NULL) {
        err = httpd_stop(webserver->server);
        webserver->server = NULL;

        event_t evt = {
            .id = EVT_WEBSERVER_ONOFF,
            .issuer = self,
            .payload = {.data.b = false, .free_fcn = NULL},
        };
        err = eventbus_post_event(&evt);
    }
    return err;
}

module_base*
webserver_create(int id, webserver_config_t* config, size_t uri_amount) {
    webserver_t* webserver = calloc(1, sizeof(webserver_t));
    memcpy(&webserver->server_config, config, sizeof(webserver_config_t));

    module_base_config_t base_config = {
        .id = id,
        .max_evts = uri_amount + EVT_WEBSERVER_USER_URI,
        .event_handler = webserver->server_config.event_handler,
    };

    ESP_ERROR_CHECK(eventbus_module_register(&webserver->module, &base_config));

    for (int i = 0; i < uri_amount + EVT_WEBSERVER_USER_URI; i++) {
        eventbus_module_subscribe(&webserver->module, id, i); // Subscribe on itself
    }
    httpd_ssl_config_t ssl_cnf = HTTPD_SSL_CONFIG_DEFAULT();
    memcpy(&webserver->https_config, &ssl_cnf, sizeof(httpd_ssl_config_t));
    webserver->https_config.httpd.max_open_sockets = webserver->server_config.max_open_sockets;
    webserver->https_config.httpd.global_user_ctx = webserver;
    webserver->https_config.httpd.global_user_ctx_free_fn = _when_httpd_stop, webserver->https_config.httpd.core_id = 1;

    const esp_timer_create_args_t timer_args = {.callback = &_shutdown_inactivity_timer_callback, .arg = webserver};
    esp_err_t err = esp_timer_create(&timer_args, &webserver->shutdown_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed shutdown init for server err=%d(%s)", err, esp_err_to_name(err));
        free(webserver);
        return NULL;
    }

    return &webserver->module;
}