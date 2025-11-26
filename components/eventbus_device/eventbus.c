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

bool
filter_component_id_exist(component_list_t* item, void* ctx) {
    int id = (int)ctx;
    return item->component->id == id;
}

bool
filter_subscription_exist(subscription_t* item, void* ctx) {
    event_handler handler = (event_handler)ctx;
    return item->handler == handler;
}

bool
filter_event_subs_exist(event_subs* item, void* ctx) {
    int id = (int)ctx;
    return item->event_id == id;
}

static void
eventbus_task(void* params) {
    esp_err_t err = ESP_OK;
    event_t event;

    while (true) {
        if (xQueueReceive(EVENTBUS.event_queue, &event, 1)) {
            subscription_t* middleware;
            SLIST_FOREACH(middleware, event.issuer->module_ptr->middlewares, next) {
                err = middleware->handler(middleware->subscriber, &event);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "event id=%d issuer %s (id=%d) middleware err=%d(%s)", event.id,
                             event.issuer->module_ptr->name, event.issuer->id, err, esp_err_to_name(err));
                }
            }

            bool subscriptions_found = false;
            event_subs* sub_list;
            SLIST_FOREACH(sub_list, event.issuer->event_subs, next) {
                if (sub_list->event_id == event.id) {
                    subscription_t* sub;
                    SLIST_FOREACH(sub, sub_list->subs, next) {
                        subscriptions_found = true;
                        err = sub->handler(sub->subscriber, &event);
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "event id=%d issuer %s (id=%d) handling err=%d(%s)", event.id,
                                     event.issuer->module_ptr->name, event.issuer->id, err, esp_err_to_name(err));
                        }
                    }
                    break;
                }
            }
            if (!subscriptions_found) {
                ESP_LOGW(TAG, "event id=%d issuer %s (id=%d) no subscribers", event.id, event.issuer->module_ptr->name,
                         event.issuer->id);
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

void
device_instance_constructor(component_base_t* base, module_base_t* module, int id) {
    base->module_ptr = module;
    base->id = id;
    SLIST_INIT(base->event_subs);
}

component_base_t*
device_module_get_component(module_base_t* module, int id) {
    component_base_t* base = NULL;
    component_list_t* component_list = NULL;
    if (id == SOLO_COMPONENT_ID) {
        if (!SLIST_EMPTY(module->components)) {
            component_list = SLIST_FIRST(module->components);
            base = component_list->component;
        }
    } else {
        if (SLIST_GET_WITH_TAIL(module->components, next, &component_list, filter_component_id_exist, (void*)id)) {
            base = component_list->component;
        }
    }
    return base;
}

esp_err_t
device_module_add_middleware(component_base_t* subscriber, module_base_t* module, event_handler h) {
    esp_err_t err = ESP_OK;

    subscription_t* tail = NULL;
    if (SLIST_GET_WITH_TAIL(module->middlewares, next, &tail, filter_subscription_exist, h)) {
        err = ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG, "middleware already registered");
    } else {
        subscription_t* new_middleware = calloc(1, sizeof(subscription_t));
        new_middleware->subscriber = subscriber;
        new_middleware->handler = h;
        if (tail) {
            SLIST_INSERT_AFTER(tail, new_middleware, next);
        } else {
            SLIST_INSERT_HEAD(module->middlewares, new_middleware, next);
        }
    }
    return err;
}

esp_err_t
device_module_add_component(int id, component_base_t* component, module_base_t* module) {
    esp_err_t err = ESP_OK;
    component_list_t* tail = NULL;

    // Init component base
    component->module_ptr = module;
    component->id = id;
    SLIST_INIT(component->event_subs);

    if (SLIST_GET_WITH_TAIL(module->components, next, &tail, filter_component_id_exist, (void*)component->id)) {
        err = ESP_ERR_NOT_ALLOWED;
    } else {
        component_list_t* new_item = calloc(1, sizeof(component_list_t));
        new_item->component = component;
        if (tail) {
            SLIST_INSERT_AFTER(tail, new_item, next);
        } else {
            SLIST_INSERT_HEAD(module->components, new_item, next);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed add component id=%d for %s err=%d(%s)", id, module->name, err, esp_err_to_name(err));
    }
    return err;
}

esp_err_t
device_subscribe(component_base_t* self, component_base_t* t, int id, event_handler h) {
    esp_err_t err = ESP_OK;

    event_subs* sub_list = NULL;
    if (!SLIST_GET_WITH_TAIL(t->event_subs, next, &sub_list, filter_event_subs_exist, (void*)id)) {
        event_subs* new_sub_list = calloc(1, sizeof(event_subs));
        new_sub_list->event_id = id;
        if (sub_list) {
            SLIST_INSERT_AFTER(sub_list, new_sub_list, next);
        } else {
            SLIST_INSERT_HEAD(t->event_subs, new_sub_list, next);
        }
        sub_list = new_sub_list;
    }

    subscription_t* new_sub = calloc(1, sizeof(subscription_t));
    new_sub->subscriber = self;
    new_sub->handler = h;

    subscription_t* sub_tail = SLIST_TAIL(sub_list->subs, next);
    if (sub_tail) {
        SLIST_INSERT_AFTER(sub_tail, new_sub, next);
    } else {
        SLIST_INSERT_HEAD(sub_list->subs, new_sub, next);
    }

    return err;
}

esp_err_t
device_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t
device_init() {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(_device_cfg_init());
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