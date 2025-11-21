#ifndef _LOGIC_H_
#define _LOGIC_H_
#include <digital_pin.h>
#include <webserver.h>
#include <ws_ledstrip.h>

enum {
    EVT_WEBSERVER_UI_ON_ROOT = EVT_WEBSERVER_USER_URI,
    EVT_WEBSERVER_UI_MAX,
};

esp_err_t web_logic();

esp_err_t led_logic();

esp_err_t pin_logic();

#endif /* _LOGIC_H_ */
