#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define TAG              "EVENTBUS"

#define EVENT_QUEUE_SIZE 20
#define EVENT_TASK_PRIO  5
#define EVENT_TASK_CORE  1

typedef struct {
    QueueHandle_t event_queue;
    TaskHandle_t event_task;
} eventbus_t;

static StaticQueue_t __eq_struct;
static uint8_t __eq_buf[EVENT_QUEUE_SIZE * sizeof(event_t)];
static eventbus_t EVENTBUS;

//===============================================================================//
//================================= Private =====================================//
//===============================================================================//
// esp_err_t
// module_create(module_base* self, module_base_config_t* config) {
//     self->id = config->id;
//     self->size = config->max_evts;
//     self->event_handler = config->event_handler;

//     self->subscriptions = malloc(self->size * sizeof(struct subscription_head));
//     if (!self->subscriptions) {
//         return ESP_ERR_NO_MEM;
//     }
//     for (int i = 0; i < self->size; i++) {
//         SLIST_INIT(&self->subscriptions[i]);
//     };
//     return ESP_OK;
// }

static void
eventbus_task(void* params) {
    esp_err_t err = ESP_OK;
    event_t event;

    while (true) {
        if (xQueueReceive(EVENTBUS.event_queue, &event, 1)) {
            middleware_list_t* middleware;
            SLIST_FOREACH(middleware, event.issuer->module_ptr->middlewares, next) {
                err = middleware->handler(&event);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "event id=%d issuer %s (instance_id=%d) middleware err=%d(%s)", event.id,
                             event.issuer->module_ptr->name, event.issuer->instance_id, err, esp_err_to_name(err));
                }
            }

            bool sunscriptions_found = false;
            event_list_t* event_list;
            SLIST_FOREACH(event_list, event.issuer->event_list, next) {
                if (event_list->event_id == event.id) {
                    subscription_t* sub;
                    sunscriptions_found = true;
                    SLIST_FOREACH(sub, event_list->subscriptions, next) {
                        err = sub->handler(sub->subscriber, &event);
                        if (err != ESP_OK) {
                            ESP_LOGW(TAG, "event id=%d issuer %s (instance_id=%d) handling err=%d(%s)", event.id,
                                     event.issuer->module_ptr->name, event.issuer->instance_id, err,
                                     esp_err_to_name(err));
                        }
                    }
                    break;
                }
            }
            if (!sunscriptions_found) {
                ESP_LOGW(TAG, "event id=%d issuer %s (instance_id=%d) no subscribers", event.id,
                         event.issuer->module_ptr->name, event.issuer->instance_id);
            }
            if (event.data_free_fcn) {
                event.data_free_fcn(event.data);
            }
        }
    }
}

//===============================================================================//
//================================= Public ======================================//
//===============================================================================//

esp_err_t
eventbus_init() {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    EVENTBUS.event_queue = xQueueCreateStatic(EVENT_QUEUE_SIZE, sizeof(event_t), __eq_buf, &__eq_struct);
    if (!EVENTBUS.event_queue) {
        return ESP_ERR_NO_MEM;
    }
    BaseType_t ret = xTaskCreatePinnedToCore(eventbus_task, "ebus", 4096, NULL, EVENT_TASK_PRIO, &EVENTBUS.event_task,
                                             EVENT_TASK_CORE);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "task create error %d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// esp_err_t
// eventbus_module_register(module_base* module, module_base_config_t* config) {
//     esp_err_t err = ESP_OK;
//     if (config->id < 0 || config->id > EVENTBUS.modules_amount) {
//         ESP_LOGE(TAG, "unknown id %d", config->id);
//         err = ESP_FAIL;
//     } else if (EVENTBUS.registry[config->id]) {
//         ESP_LOGE(TAG, "already exist id %d", config->id);
//         err = ESP_ERR_INVALID_ARG;
//     } else {
//         err = module_create(module, config);
//         if (err == ESP_OK) {
//             EVENTBUS.registry[module->id] = module;
//         }
//     }
//     return err;
// }

// module_base*
// eventbus_module_get(int id) {
//     if (id < 0 || id > EVENTBUS.modules_amount) {
//         ESP_LOGE(TAG, "unknown id %d", id);
//         return NULL;
//     }
//     return EVENTBUS.registry[id];
// }

// esp_err_t
// eventbus_module_subscribe(module_base* self, int target_id, int evt_id) {
//     module_base* addressee = eventbus_module_get(target_id);
//     if (!addressee) {
//         ESP_LOGE(TAG, "sub addressee not found for id %d", target_id);
//         return ESP_FAIL;
//     }
//     if (evt_id > addressee->size) {
//         ESP_LOGE(TAG, "unknown sub event id %d", evt_id);
//         return ESP_FAIL;
//     }
//     subscription_t* new_sub = malloc(sizeof(subscription_t));
//     if (!new_sub) {
//         return ESP_ERR_NO_MEM;
//     }
//     new_sub->module = self;
//     SLIST_INSERT_HEAD(&addressee->subscriptions[evt_id], new_sub, next);
//     return ESP_OK;
// }

void
eventbus_module_constructor(module_base_t* module, char* name) {
    module->name = name;
    SLIST_INIT(module->middlewares);
    SLIST_INIT(module->instances);
}

instance_base_t*
eventbus_module_get_instance(module_base_t* module, int id) {
    instance_base_t* base = NULL;
    instance_list_t* instance_list_item;
    SLIST_FOREACH(instance_list_item, module->instances, next) {
        if (instance_list_item->instance->instance_id == id) {
            base = instance_list_item->instance;
            break;
        }
    }
    return base;
}

esp_err_t
eventbus_module_add_middleware(module_base_t* module, middleware_handler handler) {
    esp_err_t err = ESP_OK;
    middleware_list_t* last_item = NULL;
    middleware_list_t* item;

    SLIST_FOREACH(item, module->middlewares, next) {
        if (item->handler == handler) {
            err = ESP_ERR_NOT_ALLOWED;
            ESP_LOGE(TAG, "failed add existing middleware for %s", module->name);
            return err;
        }
        if (SLIST_NEXT(item, next) == NULL) {
            last_item = item;
        }
    }

    middleware_list_t* new_list_item = calloc(1, sizeof(middleware_list_t));
    new_list_item->handler = handler;
    SLIST_INSERT_AFTER(last_item, new_list_item, next);
    return err;
}

esp_err_t
eventbus_module_add_instance(module_base_t* module, instance_base_t* instance) {
    esp_err_t err = ESP_OK;
    instance_list_t* last_item = NULL;
    instance_list_t* item;

    SLIST_FOREACH(item, module->instances, next) {
        if (item->instance == instance) {
            err = ESP_ERR_NOT_ALLOWED;
            ESP_LOGE(TAG, "instance_id=%d (%s) already exist", instance->instance_id, module->name);
            return err;
        }
        if (SLIST_NEXT(item, next) == NULL) {
            last_item = item;
        }
    }

    instance_list_t* new_list_item = calloc(1, sizeof(instance_list_t));
    new_list_item->instance = instance;
    SLIST_INSERT_AFTER(last_item, new_list_item, next);
    return err;
}

esp_err_t
eventbus_subscribe(instance_base_t* self, instance_base_t* target, int event_id, event_handler handler) {
    esp_err_t err = ESP_OK;
    bool list_item_found = false;

    event_list_t* event_list_item;

    SLIST_FOREACH(event_list_item, target->event_list, next) {
        if (event_list_item->event_id == event_id) {
            list_item_found = true;
            //__insert
            subscription_t* new_sub_item = calloc(1, sizeof(subscription_t));
            new_sub_item->subscriber = self;
            new_sub_item->handler = handler;

            subscription_t* last_sub_item = NULL;
            subscription_t* sub_item;
            SLIST_FOREACH(sub_item, event_list_item->subscriptions, next) {
                if (SLIST_NEXT(sub_item, next) == NULL) {
                    last_sub_item = sub_item;
                }
            }
            if (last_sub_item) {
                SLIST_INSERT_AFTER(last_sub_item, new_sub_item, next);
            }
            break;
        }
    }
    if (!list_item_found) {
        event_list_t* new_event_list_item = calloc(1, sizeof(event_list_t));
        new_event_list_item->event_id = event_id;
        SLIST_INSERT_HEAD(target->event_list, new_event_list_item, next);
        SLIST_INSERT_AFTER(new_event_list_item, new_sub_item, next);
        //__insert
    }
}

esp_err_t
eventbus_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->instance_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}