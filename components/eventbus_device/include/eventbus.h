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

#include <config_entry.h>

#define SOLO_COMPONENT_ID 0

typedef struct component_base component_base_t;
typedef struct module_base module_base_t;

// Declare a mudule with the name, should be placed in .h per every module
#define DEVICE_MODULE_DECLARE(id) extern module_base_t* id

// Bootstrap the mudule by name in .c, requires DEVICE_MODULE_DECLARE(name) beforehand in .h
#define DEVICE_MODULE_REGISTER(id)                                                                                     \
    module_base_t id##_obj = {.name = #id, .middlewares = NULL, .components = NULL};                                   \
    module_base_t* id = &id##_obj

// SLIST find tail macro. Return pointer to tail item or NULL if list is empty
#define SLIST_TAIL(head, field)                                                                                        \
    ({                                                                                                                 \
        __typeof__(SLIST_FIRST(head)) _it, _last = NULL;                                                               \
        if (!SLIST_EMPTY(head)) {                                                                                      \
            SLIST_FOREACH(_it, head, field) { _last = _it; }                                                           \
        }                                                                                                              \
        _last;                                                                                                         \
    })

// SLIST find macro with filter callback.
// Footprint for callback: bool fcn(component_type* item, void* ctx), where component_type should be of list item type
// Return true if filter got triggered and with a pointer to the item in *res_or_tail*
// Return false if filter not triggered and *res_or_tail* NULL for case list is empty or a pointer to a tail item
#define SLIST_GET_WITH_TAIL(head, field, res_or_tail, callback, ctx)                                                   \
    ({                                                                                                                 \
        __typeof__(SLIST_FIRST(head)) _it, _last = NULL;                                                               \
        bool _found = false;                                                                                           \
        if (!SLIST_EMPTY(head)) {                                                                                      \
            SLIST_FOREACH(_it, head, field) {                                                                          \
                _last = _it;                                                                                           \
                if (callback(_it, ctx)) {                                                                              \
                    _found = true;                                                                                     \
                    *(res_or_tail) = _it;                                                                              \
                    break;                                                                                             \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        if (!_found) {                                                                                                 \
            *(res_or_tail) = _last;                                                                                    \
        }                                                                                                              \
        _found;                                                                                                        \
    })

//===========================================================================//
//====================== EVENT AND SUBSCRIPTION =============================//
//===========================================================================//

typedef struct {
    component_base_t* issuer;
    int id;
    void* data;
    size_t data_size;
    void (*data_free_fcn)(void* data);
} event_t;

typedef esp_err_t (*event_handler)(component_base_t* subscriber, event_t* event);

typedef struct subscription {
    component_base_t* subscriber;
    event_handler handler;
    SLIST_ENTRY(subscription) next;
} subscription_t;

SLIST_HEAD(subscription_head, subscription);

typedef struct event_subs {
    int event_id;
    struct subscription_head* subs;
    SLIST_ENTRY(event_subs) next;
} event_subs;

SLIST_HEAD(event_subs_head, event_subs);

//===========================================================================//
//========================== MIDDLEWARE =====================================//
//===========================================================================//

typedef esp_err_t (*middleware_handler)(event_t* event);

typedef struct middleware_list {
    middleware_handler handler;
    SLIST_ENTRY(middleware_list) next;
} middleware_list_t;

SLIST_HEAD(middleware_list_head, middleware_list);

//===========================================================================//
//========================== COMPONENT ======================================//
//===========================================================================//

struct component_base {
    module_base_t* module_ptr;
    int id;
    struct event_subs_head* event_subs;
};

typedef struct component_list {
    component_base_t* component;
    SLIST_ENTRY(component_list) next;
} component_list_t;

SLIST_HEAD(component_list_head, component_list);

//===========================================================================//
//========================== MODULE =========================================//
//===========================================================================//

struct module_base {
    char* name;
    struct middleware_list_head* middlewares;
    struct component_list_head* components;
};

//===========================================================================//

esp_err_t device_init();

esp_err_t device_module_add_middleware(module_base_t* module, middleware_handler handler);

esp_err_t device_module_add_component(int id, component_base_t* component, module_base_t* module);

component_base_t* device_module_get_component(module_base_t* module, int id);

esp_err_t device_module_subscribe(component_base_t* self, module_base_t* module, int id, event_handler h);

esp_err_t device_module_subscribe_to(component_base_t* self, component_base_t* t, int id, event_handler h);

esp_err_t device_post_event(event_t* event);

#endif /* _EVENTBUS_H_ */