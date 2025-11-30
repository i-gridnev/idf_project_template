#ifndef _DEVICE_CONFIG_H_
#define _DEVICE_CONFIG_H_

#include <digital_pin.h>
#include <webserver.h>
#include <wifi_network.h>
#include <ws_ledstrip.h>

enum digital_pin_ids {
    PIN_BTN,
    PIN_MAX,
};

enum led_ids {
    LED_WIFI_SMART,
    LED_2,
    LED_3,
    LED_MAX,
};

enum config_entry_ids {
    CONFIG_WIFI_SSID = 0,
    CONFIG_WIFI_PASSWORD,
    CONFIG_MAX,
};

enum {
    EVT_WEBSERVER_UI_ON_ROOT = EVT_WEBSERVER_USER_URI,
    EVT_WEBSERVER_UI_ON_TEST,
    EVT_WEBSERVER_UI_ON_TEST2,
    EVT_WEBSERVER_UI_ON_TEST3,
};

esp_err_t web_logic();

esp_err_t led_logic();

esp_err_t pin_logic();

#endif /* _DEVICE_CONFIG_H_ */
