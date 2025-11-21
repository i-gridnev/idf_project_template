/*
 * Component: WiFi system
 * 
*/

#ifndef _WIFI_NETWORK_H_
#define _WIFI_NETWORK_H_

#include <stdbool.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "eventbus.h"

typedef struct {
    module_base_t base;
} wifi_module_t;

extern wifi_module_t* WIFI_MODULE;

//=============================================================//
//============= EVENT DESCRIPTION =============================//
enum {
    EVT_WIFI_READY,
    EVT_WIFI_STA_CONNECTION,
    EVT_WIFI_STA_TRYING,
    //////////////////////////////
    EVT_WIFI_MAX,
};

typedef union {
    void* raw;
    bool success;
} wifi_event_data_t;

//=============================================================//
//==================== WIFI ENTITY ============================//
typedef struct {
    wifi_mode_t mode;
    int reconnect_interval_ms;
    int reconnect_attempts;
} wifi_component_config_t;

typedef struct {
    instance_base_t base;
    wifi_component_config_t config;
    EventGroupHandle_t status;
    int connect_attempts;
    esp_timer_handle_t reconnect_timer;
} wifi_component_t;

wifi_component_t* wifi_network_create(wifi_component_config_t* config);

esp_err_t wifi_network_STA_connect(char* STA_ssid, char* STA_pass);

bool wifi_network_await_STA_connect(int timeout_ms);

esp_err_t wifi_network_STA_disconnect();
bool wifi_network_await_STA_disconnect(int timeout_ms);

bool is_wifi_network_STA_connected();

bool is_wifi_network_STA_trying();

#endif /* _WIFI_NETWORK_H_ */