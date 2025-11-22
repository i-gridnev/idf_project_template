/*
 * Component: Webserver
 * 
*/

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <esp_https_server.h>
#include <esp_timer.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    module_base_t base;
} webserver_module_t;

extern webserver_module_t* WEBSERVER_MODULE;

//=============================================================//
//============= EVENT DESCRIPTION =============================//

enum {
    EVT_WEBSERVER_INACTIVE,
    EVT_WEBSERVER_ON,
    EVT_WEBSERVER_OFF,
    //////////////////////////////
    EVT_WEBSERVER_USER_URI,
};

typedef struct {
    httpd_req_t* req;

    struct {
        char* ptr;
        size_t size;
        bool persistent;
    } buffer;
} webserver_req_buffer_t;

//=============================================================//
//==================== WEBSERVER ENTITY =======================//

typedef struct {
    const char* uri;
    httpd_method_t method;
    int event_id;
    event_handler handler;
} webserver_uri_t;

typedef struct {
    uint16_t max_open_sockets;
    int inactive_shutdown_ms;
    webserver_uri_t* uris;
    size_t uris_size;
} webserver_component_config_t;

typedef struct {
    instance_base_t base;
    // char* hostname;
    httpd_handle_t server;
    webserver_component_config_t server_config;
    httpd_ssl_config_t https_config;
    esp_timer_handle_t shutdown_timer;
    bool is_started;
} webserver_component_t;

webserver_component_t* webserver_create(webserver_component_config_t* config);

esp_err_t webserver_start_http();

esp_err_t webserver_stop();

esp_err_t webserver_enqueue_response(webserver_req_buffer_t* response);

#endif /* _WEBSERVER_H_ */