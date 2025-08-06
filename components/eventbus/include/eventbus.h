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

#define ANY_MODULE_ID -1

typedef struct event event_t;
typedef struct module_base module_base;
typedef esp_err_t (*event_handler)(module_base* self, event_t* event);

typedef struct subscription {
    module_base* module;
    SLIST_ENTRY(subscription) next;
} subscription_t;

SLIST_HEAD(subscription_head, subscription);

struct module_base {
    int id;
    size_t size;
    event_handler event_handler;
    struct subscription_head* subscriptions;
};

typedef struct {
    int id;
    size_t max_evts;
    event_handler event_handler;
} module_base_config_t;

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

            struct {
                uint16_t u1;
                uint16_t u2;
            } double_u16;
        } data;

        size_t size;
        void (*free_fcn)(void* data);
    } payload;
} event_t;

esp_err_t eventbus_init(size_t modules_amount);

esp_err_t eventbus_module_register(module_base* module, module_base_config_t* config);

module_base* eventbus_module_get(int id);

esp_err_t eventbus_module_subscribe(module_base* self, int target_id, int evt_id);

esp_err_t eventbus_post_event(event_t* event);

#endif /* _EVENTBUS_H_ */