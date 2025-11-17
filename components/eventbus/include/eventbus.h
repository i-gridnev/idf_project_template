/*
 * Component: Event Bus
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
*/

#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

#include <stdbool.h>
#include <stdio.h>
#include <sys/queue.h>
#include "esp_err.h"

typedef struct instance_base instance_base_t;
typedef struct module_base module_base_t;

typedef struct {
    instance_base_t* issuer;
    int id;
    void* data;
    size_t data_size;
    void (*data_free_fcn)(void* data);
} event_t;

typedef esp_err_t (*event_handler)(instance_base_t* subscriber, event_t* event);

typedef esp_err_t (*middleware_handler)(event_t* event);

typedef struct subscription {
    instance_base_t* subscriber;
    event_handler handler;
    SLIST_ENTRY(subscription) next;
} subscription_t;

SLIST_HEAD(subscription_head, subscription);

typedef struct event_list {
    int event_id;
    struct subscription_head* subscriptions;
    SLIST_ENTRY(event_list) next;
} event_list_t;

SLIST_HEAD(event_list_head, event_list);

typedef struct instance_list {
    instance_base_t* instance;
    SLIST_ENTRY(instance_list) next;
} instance_list_t;

SLIST_HEAD(instance_list_head, instance_list);

typedef struct middleware_list {
    middleware_handler handler;
    SLIST_ENTRY(middleware_list) next;
} middleware_list_t;

SLIST_HEAD(middleware_list_head, middleware_list);

struct instance_base {
    module_base_t* module_ptr;
    int instance_id;
    struct event_list_head* event_list;
};

struct module_base {
    char* name;
    struct middleware_list_head* middlewares;
    struct instance_list_head* instances;
};

esp_err_t eventbus_init();

void eventbus_module_constructor(module_base_t* base, char* name);
esp_err_t eventbus_module_add_middleware(module_base_t* module, middleware_handler handler);
esp_err_t eventbus_module_add_instance(module_base_t* module, instance_base_t* instance);
instance_base_t* eventbus_module_get_instance(module_base_t* module, int id);

void eventbus_instance_constructor(instance_base_t* base, module_base_t* module, int id);
esp_err_t eventbus_instance_subscribe(instance_base_t* self, instance_base_t* t, int id, event_handler h);

esp_err_t eventbus_post_event(event_t* event);

#endif /* _EVENTBUS_H_ */