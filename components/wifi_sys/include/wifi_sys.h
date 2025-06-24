/*
 * Component: WiFi system
 * 
*/

#ifndef _WIFI_SYS_H_
#define _WIFI_SYS_H_

#include <stdbool.h>
#include "esp_err.h"

#include "eventbus.h"

enum {
    EVT_WIFI_AP_ON,
    EVT_WIFI_AP_OFF,
    EVT_WIFI_STA_ON,
    EVT_WIFI_STA_CONNECTED,
    EVT_WIFI_STA_FAILED,
    EVT_WIFI_STA_OFF,
    //////////////////////////////
    EVT_WIFI_MAX,
};

typedef struct {
    int reconnect_interval_ms;
    int reconnect_attempts;
} wifi_sys_config_t;

typedef void* wifi_sys_module;

wifi_sys_module* wifi_sys_create(int id, wifi_sys_config_t* config);

#endif /* _WIFI_SYS_H_ */