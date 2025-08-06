#ifndef _WEB_LOGIC_H_
#define _WEB_LOGIC_H_

#include <webserver.h>

enum {
    EVT_WEBSERVER_UI_ON_ROOT = EVT_WEBSERVER_USER_URI,
    EVT_WEBSERVER_UI_MAX,
};

esp_err_t web_logic();

#endif /* _WEB_LOGIC_H_ */
