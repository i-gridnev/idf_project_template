#ifndef _WEB_UI_H_
#define _WEB_UI_H_

#include "esp_err.h"

#include <webserver.h>

enum {
    EVT_WEBSERVER_UI_ON_ROOT = EVT_WEBSERVER_USER_URI,
    EVT_WEBSERVER_UI_MAX,
};

void web_ui_create(int id);

#endif /* _WEB_UI_H_ */
