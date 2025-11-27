#include <esp_log.h>
// #include "mdns.h"

#include "eventbus.h"
#include "webserver.h"

#define SEND_CHANK_SIZE                 40000 // RAM buffer for sending data in bytes (!caution!)
#define MAX_RECV_CONTENT_SIZE           8000

#define HTTP_413_CONTENT_TOO_LARGE      "413 Content Too Large"
#define HTTP_413_MSG                    "Maximum allowed size reached"
#define HTTP_415_UNSUPPORTED_MEDIA_TYPE "415 Unsupported Media Type"
#define HTTP_415_MSG                    "Unable to parse the request"

#define TAG                             "WEB"

DEVICE_MODULE_REGISTER(WEBSERVER_MODULE);

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
    webserver_component_t* webserver = (webserver_component_t*)arg;
    event_t evt = {
        .id = EVT_WEBSERVER_INACTIVE,
        .issuer = &webserver->base,
        .data_free_fcn = NULL,
    };
    device_post_event(&evt);
    esp_timer_start_once(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
}

static void
_free_action(void* webserver_req_buffer) {
    webserver_req_buffer_t* req_buffer = (webserver_req_buffer_t*)webserver_req_buffer;
    if (!req_buffer->buffer.persistent) {
        free(req_buffer->buffer.ptr);
    }
    free(req_buffer);
}

static esp_err_t
base_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "request:'%s'", req->uri);

    webserver_component_t* webserver = httpd_get_global_user_ctx(req->handle);

    if (webserver->server_config.inactive_shutdown_ms > 0) {
        esp_err_t err =
            esp_timer_restart(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "inactivity timer update err=%d(%s)", err, esp_err_to_name(err));
        }
        ESP_LOGD(TAG, "inactivity updated");
    }

    webserver_req_buffer_t* request = calloc(1, sizeof(webserver_req_buffer_t));
    request->req = req;
    request->buffer.persistent = false;
    if (req->method != HTTP_GET) {
        request->buffer.ptr = _read_payload_raw(req);
        if (request->buffer.ptr == NULL) { // error codes are handled while read
            free(request);
            return ESP_FAIL;
        }
        request->buffer.size = strlen(request->buffer.ptr);
    }

    if (httpd_req_async_handler_begin(req, &request->req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        _free_action(request);
        return ESP_FAIL;
    }
    event_t evt = {
        .id = *(int*)req->user_ctx,
        .issuer = &webserver->base,
        .data = (void*)request,
        .data_size = sizeof(webserver_req_buffer_t),
        .data_free_fcn = _free_action,
    };
    return device_post_event(&evt);
}

static void
_async_send_handler(void* arg) {
    webserver_req_buffer_t* async_response = (webserver_req_buffer_t*)arg;
    esp_err_t err = _send_chunked(async_response->req, async_response->buffer.ptr, async_response->buffer.size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to respond to '%s' err=%d(%s)", async_response->req->uri, err, esp_err_to_name(err));
        _free_action(async_response);
        return;
    }
    err = httpd_req_async_handler_complete(async_response->req);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to complete async respond err=%d(%s)", err, esp_err_to_name(err));
    }
    _free_action(async_response);
}

static void
_when_httpd_stop(void* global_user_ctx) {
    webserver_component_t* webserver = (webserver_component_t*)global_user_ctx;
    esp_timer_stop(webserver->shutdown_timer);
}

esp_err_t
webserver_enqueue_response(webserver_req_buffer_t* response) {
    webserver_req_buffer_t* async_response = calloc(1, sizeof(webserver_req_buffer_t));
    async_response->req = response->req;
    async_response->buffer.ptr = response->buffer.ptr; // safe if persistent/static
    async_response->buffer.size = response->buffer.size;
    async_response->buffer.persistent = response->buffer.persistent;
    esp_err_t err = httpd_queue_work(response->req->handle, _async_send_handler, async_response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enqueue async response err=%d(%s)", err, esp_err_to_name(err));
        _free_action(async_response);
    }
    return err;
}

esp_err_t
webserver_start_http() {
    esp_err_t err = ESP_FAIL;
    webserver_component_t* webserver =
        (webserver_component_t*)device_module_get_component(WEBSERVER_MODULE, SOLO_COMPONENT_ID);

    if (webserver->server == NULL) {
        webserver->https_config.httpd.server_port = 80;
        webserver->https_config.httpd.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT;
        webserver->https_config.httpd.max_uri_handlers = webserver->server_config.uris_size;

        err = httpd_start(&webserver->server, &webserver->https_config.httpd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed start http server err=%d(%s)", err, esp_err_to_name(err));
        } else {
            for (int i = 0; i < webserver->server_config.uris_size; i++) {
                httpd_uri_t uri = {
                    .uri = webserver->server_config.uris[i].uri,
                    .method = webserver->server_config.uris[i].method,
                    .handler = base_handler,
                    .user_ctx = &webserver->server_config.uris[i].event_id,
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
                    .id = EVT_WEBSERVER_ON,
                    .issuer = &webserver->base,
                    .data_free_fcn = NULL,
                };
                device_post_event(&evt);
            }
            if (webserver->server_config.inactive_shutdown_ms) {
                esp_timer_start_once(webserver->shutdown_timer, webserver->server_config.inactive_shutdown_ms * 1000);
            }
        }
    }
    return err;
}

esp_err_t
webserver_stop() {
    esp_err_t err = ESP_FAIL;
    webserver_component_t* webserver =
        (webserver_component_t*)device_module_get_component(WEBSERVER_MODULE, SOLO_COMPONENT_ID);

    if (webserver->server != NULL) {
        err = httpd_stop(webserver->server);
        webserver->server = NULL;
        webserver->is_started = false;
        event_t evt = {
            .id = EVT_WEBSERVER_OFF,
            .issuer = &webserver->base,
            .data_free_fcn = NULL,
        };
        device_post_event(&evt);
    }
    return err;
}

webserver_component_t*
webserver_create(webserver_component_config_t* config) {
    esp_err_t err = ESP_OK;
    webserver_component_t* webserver = calloc(1, sizeof(webserver_component_t));
    memcpy(&webserver->server_config, config, sizeof(webserver_component_config_t));
    httpd_ssl_config_t ssl_cnf = HTTPD_SSL_CONFIG_DEFAULT();
    memcpy(&webserver->https_config, &ssl_cnf, sizeof(httpd_ssl_config_t));
    webserver->https_config.httpd.max_open_sockets = webserver->server_config.max_open_sockets;
    webserver->https_config.httpd.global_user_ctx = webserver;
    webserver->https_config.httpd.global_user_ctx_free_fn = _when_httpd_stop;
    webserver->https_config.httpd.core_id = 1;
    const esp_timer_create_args_t timer_args = {.callback = &_shutdown_inactivity_timer_callback, .arg = webserver};
    err = esp_timer_create(&timer_args, &webserver->shutdown_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed shutdown init for server err=%d(%s)", err, esp_err_to_name(err));
        free(webserver);
        return NULL;
    }

    if (device_module_add_component(SOLO_COMPONENT_ID, &webserver->base, WEBSERVER_MODULE) != ESP_OK) {
        free(webserver);
        return NULL;
    }

    for (webserver_uri_t* uri = webserver->server_config.uris;
         uri < webserver->server_config.uris + webserver->server_config.uris_size; uri++) {
        device_subscribe(&webserver->base, &webserver->base, uri->event_id, uri->handler); // Subscribe on itself
    }

    return webserver;
}