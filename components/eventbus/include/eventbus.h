/*
 * Component: Event Bus
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
*/

#ifndef _EVENT_BUS_H_
#define _EVENT_BUS_H_

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int id;
} ModuleBase;

typedef struct {
    int id;

    void* payload;
    void (*free_payload)(void* payload);

    ModuleBase* issuer;
    int* target_module_id;
    size_t target_num;
    int target_group;
} event_t;

void eventbus_init(size_t modules_anount);

#endif /* _EVENT_BUS_H_ */