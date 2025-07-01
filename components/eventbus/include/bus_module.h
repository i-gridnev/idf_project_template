
#ifndef _BUS_MODULE_H_
#define _BUS_MODULE_H_

#include <stdio.h>
#include <sys/queue.h>
#include "esp_err.h"

typedef struct event event_t;
typedef struct module_base module_base;

typedef struct {
    int id;
    size_t max_evts;
    esp_err_t (*event_handler)(module_base* self, event_t* event);
} module_base_config_t;

typedef struct subscription {
    module_base* module;
    SLIST_ENTRY(subscription) next;
} subscription_t;

SLIST_HEAD(subscription_head, subscription);

struct module_base {
    int id;

    struct {
        struct subscription_head* on_evt;
        size_t size;
    } subscriptions;

    esp_err_t (*event_handler)(module_base* self, event_t* event);
};

esp_err_t module_create(module_base* self, module_base_config_t* config);

esp_err_t module_subscribe(module_base* self, int target_id, int evt_id);

#endif /* _BUS_MODULE_H_ */