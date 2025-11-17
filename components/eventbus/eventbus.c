#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define TAG              "EVENTBUS"

#define EVENT_QUEUE_SIZE 20
#define EVENT_TASK_PRIO  5
#define EVENT_TASK_CORE  1

// Return pointer to tail item or NULL if list is empty
#define SLIST_TAIL(head, field)                                                                                        \
    ({                                                                                                                 \
        __typeof__(SLIST_FIRST(head)) _it, _last = NULL;                                                               \
        if (!SLIST_EMPTY(head)) {                                                                                      \
            SLIST_FOREACH(_it, head, field) { _last = _it; }                                                           \
        }                                                                                                              \
        _last;                                                                                                         \
    })

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

bool
filter_instance_id_exist(instance_list_t* item, void* ctx) {
    int id = (int)ctx;
    return item->instance->instance_id == id;
}

bool
filter_middleware_exist(middleware_list_t* item, void* ctx) {
    middleware_handler handler = (middleware_handler)ctx;
    return item->handler == handler;
}

bool
filter_event_list_exist(event_list_t* item, void* ctx) {
    int id = (int)ctx;
    return item->event_id == id;
}

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
                    SLIST_FOREACH(sub, event_list->subscriptions, next) {
                        sunscriptions_found = true;
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

void
eventbus_module_constructor(module_base_t* base, char* name) {
    base->name = name;
    SLIST_INIT(base->middlewares);
    SLIST_INIT(base->instances);
}

void
eventbus_instance_constructor(instance_base_t* base, module_base_t* module, int id) {
    base->module_ptr = module;
    base->instance_id = id;
    SLIST_INIT(base->event_list);
}

instance_base_t*
eventbus_module_get_instance(module_base_t* module, int id) {
    instance_base_t* base = NULL;
    instance_list_t* instance_list = NULL;
    if (SLIST_GET_WITH_TAIL(module->instances, next, &instance_list, filter_instance_id_exist, (void*)id)) {
        base = instance_list->instance;
    }
    return base;
}

esp_err_t
eventbus_module_add_middleware(module_base_t* module, middleware_handler handler) {
    esp_err_t err = ESP_OK;
    middleware_list_t* tail = NULL;
    if (SLIST_GET_WITH_TAIL(module->middlewares, next, &tail, filter_middleware_exist, handler)) {
        err = ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG, "middleware already registered");
    } else {
        middleware_list_t* new_middleware = calloc(1, sizeof(middleware_list_t));
        new_middleware->handler = handler;
        if (tail) {
            SLIST_INSERT_AFTER(tail, new_middleware, next);
        } else {
            SLIST_INSERT_HEAD(module->middlewares, new_middleware, next);
        }
    }
    return err;
}

esp_err_t
eventbus_module_add_instance(module_base_t* module, instance_base_t* instance) {
    esp_err_t err = ESP_OK;
    instance_list_t* tail = NULL;
    if (SLIST_GET_WITH_TAIL(module->instances, next, &tail, filter_instance_id_exist, (void*)instance->instance_id)) {
        err = ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG, "instance_id=%d already registered", instance->instance_id);
    } else {
        instance_list_t* new_item = calloc(1, sizeof(instance_list_t));
        new_item->instance = instance;
        if (tail) {
            SLIST_INSERT_AFTER(tail, new_item, next);
        } else {
            SLIST_INSERT_HEAD(module->instances, new_item, next);
        }
    }
    return err;
}

esp_err_t
eventbus_instance_subscribe(instance_base_t* self, instance_base_t* t, int id, event_handler h) {
    esp_err_t err = ESP_OK;

    event_list_t* event_list = NULL;
    if (!SLIST_GET_WITH_TAIL(t->event_list, next, &event_list, filter_event_list_exist, (void*)id)) {
        event_list_t* new_event_list = calloc(1, sizeof(event_list_t));
        new_event_list->event_id = id;
        if (event_list) {
            SLIST_INSERT_AFTER(event_list, new_event_list, next);
        } else {
            SLIST_INSERT_HEAD(t->event_list, new_event_list, next);
        }
        event_list = new_event_list;
    }

    subscription_t* new_sub = calloc(1, sizeof(subscription_t));
    new_sub->subscriber = self;
    new_sub->handler = h;

    subscription_t* sub_tail = SLIST_TAIL(event_list->subscriptions, next);
    if (sub_tail) {
        SLIST_INSERT_AFTER(sub_tail, new_sub, next);
    } else {
        SLIST_INSERT_HEAD(event_list->subscriptions, new_sub, next);
    }

    return err;
}

esp_err_t
eventbus_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->instance_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}