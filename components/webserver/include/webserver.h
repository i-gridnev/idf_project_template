/*
 * Component: Webserver
 * 
*/

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <esp_https_server.h>
#include <stdbool.h>
#include "esp_err.h"

enum {
    EVT_WEBSERVER_INACTIVE,
    EVT_WEBSERVER_ONOFF,
    //////////////////////////////
    EVT_WEBSERVER_USER_URI,
};

typedef struct {
    httpd_req_t* req;
    char* buffer;
    size_t buffer_size;
    bool need_free;
} webserver_action_t;

typedef struct {
    const char* uri;
    httpd_method_t method;
    esp_err_t (*handler)(webserver_action_t* action);
} webserver_uri_t;

typedef struct {
    uint16_t max_open_sockets;
    event_handler event_handler;
    int inactive_shutdown_ms;
} webserver_config_t;

module_base* webserver_create(int id, webserver_config_t* config, size_t uri_amount);

esp_err_t webserver_start_http(module_base* self, webserver_uri_t* uri_array, size_t uri_amount);

esp_err_t webserver_stop(module_base* self);

esp_err_t webserver_enqueue_response(webserver_action_t* response);

bool is_webserver_started(module_base* self);

#endif /* _WEBSERVER_H_ */