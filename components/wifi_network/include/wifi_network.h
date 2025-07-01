/*
 * Component: WiFi system
 * 
*/

#ifndef _WIFI_NETWORK_H_
#define _WIFI_NETWORK_H_

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

#include "eventbus.h"

enum {
    EVT_WIFI_STA_CONNECTION,
    EVT_WIFI_STA_TRYING,
    //////////////////////////////
    EVT_WIFI_MAX,
};

//=========== Payload definition ===============

typedef bool evt_wifi_sta_connection;
typedef bool evt_wifi_sta_trying;

//==============================================

typedef struct {
    wifi_mode_t mode;
    int reconnect_interval_ms;
    int reconnect_attempts;
} wifi_network_config_t;

typedef void* wifi_network_module;

wifi_network_module* wifi_network_create(int id, wifi_network_config_t* config);

esp_err_t wifi_network_STA_connect(char* STA_ssid, char* STA_pass);
bool wifi_network_await_STA_connect(int timeout_ms);

esp_err_t wifi_network_STA_disconnect();
bool wifi_network_await_STA_disconnect(int timeout_ms);

bool is_wifi_network_STA_connected();

bool is_wifi_network_STA_trying();

#endif /* _WIFI_NETWORK_H_ */