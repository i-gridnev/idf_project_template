/*
 * Component: Event Bus
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
*/

#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

#include <stdbool.h>
#include "esp_err.h"

#include "eventbus_config.h"

typedef struct event event_t;

typedef struct {
    int id;
    int group_id;
    esp_err_t (*event_handler)(event_t* event);
} ModuleBase;

struct event {
    int id;
    ModuleBase* issuer;

    struct {
        void* data;
        size_t size;
        void (*free_fcn)(void* data);
    } payload;

    struct {
        int* module_id;
        size_t amount;
        int group_id;
    } target;
};

void eventbus_init();
void eventbus_print_layput();
void eventbus_register(ModuleBase* module, group_id id);

#endif /* _EVENTBUS_H_ */