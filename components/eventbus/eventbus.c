#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"
#include "membership.h"


typedef struct {
    ModuleBase* registry[MODULES_MAX];
    struct group_head groups[GROUPS_MAX];
    QueueHandle_t event_queue;
} eventbus_t;

static StaticQueue_t __eq_struct;
static uint8_t __eq_buf[EVT_QUEUE_SIZE * sizeof(event_t)];
static eventbus_t EVENTBUS;

void
eventbus_init() {
    EVENTBUS.event_queue = xQueueCreateStatic(EVT_QUEUE_SIZE, sizeof(event_t), __eq_buf, &__eq_struct);
    for (int i = 0; i < GROUPS_MAX; i++) {
        SLIST_INIT(&EVENTBUS.groups[i]);
    }
}

void
eventbus_register(ModuleBase* module, group_id id) {
    group_member_t* node = malloc(sizeof(group_member_t));
    node->mod = module;
    SLIST_INSERT_HEAD(&EVENTBUS.groups[id], node, next);
}

void
eventbus_print_layput() {
    for (int g = 0; g < GROUPS_MAX; g++) {
        printf("Group %d members:\n", g);
        group_member_t* it;
        SLIST_FOREACH(it, &EVENTBUS.groups[g], next) { printf("   module_id = %d\n", it->mod->id); }
        printf("\n");
    }
}