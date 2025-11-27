/*
 * Component: Event Bus
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
*/

#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"

#include <config_entry.h>
#include <slist_patch.h>

typedef struct component_base component_base_t;
typedef struct module_base module_base_t;

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
    struct subscription_head subs;
    SLIST_ENTRY(event_subs) next;
} event_subs;

SLIST_HEAD(event_subs_head, event_subs);

//===========================================================================//
//========================== COMPONENT ======================================//
//===========================================================================//

#define SOLO_COMPONENT_ID -1

struct component_base {
    module_base_t* module_ptr;
    int id;
    struct event_subs_head event_subs;
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
    struct subscription_head middlewares;
    struct component_list_head components;
};

// Declare a mudule with the name, should be placed in .h per every module
#define DEVICE_MODULE_DECLARE(id) extern module_base_t* id

// Bootstrap the mudule by name in .c, requires DEVICE_MODULE_DECLARE(name) beforehand in .h
#define DEVICE_MODULE_REGISTER(id)                                                                                     \
    static module_base_t id##_obj = {.name = #id, .middlewares.slh_first = NULL, .components.slh_first = NULL};        \
    module_base_t* id = &id##_obj

//===========================================================================//

esp_err_t device_init();

esp_err_t device_module_add_middleware(component_base_t* subscriber, module_base_t* module, event_handler h);

esp_err_t device_module_add_component(int id, component_base_t* component, module_base_t* module);

component_base_t* device_module_get_component(module_base_t* module, int id);

esp_err_t device_subscribe(component_base_t* subscriber, component_base_t* t, int id, event_handler h);

esp_err_t device_post_event(event_t* event);

#endif /* _EVENTBUS_H_ */