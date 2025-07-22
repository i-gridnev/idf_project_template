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

#define ANY_MODULE_ID -1

typedef struct event {
    int id;
    module_base* issuer;

    struct {
        union sruct {
            void* ptr;
            char* str;
            int i32;
            uint16_t u16;
            float f32;
            bool b;
        } data;

        size_t size;
        void (*free_fcn)(void* data);
    } payload;
} event_t;

esp_err_t eventbus_init(size_t modules_amount);

esp_err_t eventbus_module_register(module_base* module);

module_base* eventbus_module_get(int id);

esp_err_t eventbus_post_event(event_t* event);

#endif /* _EVENTBUS_H_ */