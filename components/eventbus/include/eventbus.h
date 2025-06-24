/*
 * Component: Event Bus
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
*/

#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

#include <stdbool.h>
#include "esp_err.h"

#include "bus_module.h"
#include "eventbus_config.h"

#define ANY_MODULE_ID -1

typedef struct event {
    int id;
    module_base* issuer;

    struct {
        void* data;
        size_t size;
        void (*free_fcn)(void* data);
    } payload;
} event_t;

void eventbus_init();

esp_err_t eventbus_module_register(module_base* module);

module_base* eventbus_module_get(int id);

#endif /* _EVENTBUS_H_ */