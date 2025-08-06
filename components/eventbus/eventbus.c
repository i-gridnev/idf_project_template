#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define TAG              "EVENTBUS"

#define EVENT_QUEUE_SIZE 20
#define EVENT_TASK_PRIO  5
#define EVENT_TASK_CORE  1

typedef struct {
    module_base** registry;
    size_t modules_amount;
    QueueHandle_t event_queue;
    TaskHandle_t event_task;
} eventbus_t;

static StaticQueue_t __eq_struct;
static uint8_t __eq_buf[EVENT_QUEUE_SIZE * sizeof(event_t)];
static eventbus_t EVENTBUS;

//===============================================================================//
//================================= Private =====================================//
//===============================================================================//
esp_err_t
module_create(module_base* self, module_base_config_t* config) {
    self->id = config->id;
    self->size = config->max_evts;
    self->event_handler = config->event_handler;

    self->subscriptions = malloc(self->size * sizeof(struct subscription_head));
    if (!self->subscriptions) {
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < self->size; i++) {
        SLIST_INIT(&self->subscriptions[i]);
    };
    return ESP_OK;
}

static void
eventbus_task(void* params) {
    event_t event;

    while (true) {
        if (xQueueReceive(EVENTBUS.event_queue, &event, 1)) {
            subscription_t* sub;
            SLIST_FOREACH(sub, &event.issuer->subscriptions[event.id], next) {
                if (sub->module->event_handler != NULL) {
                    esp_err_t err = sub->module->event_handler(sub->module, &event);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "event id=%d issuer=%d handling err=%d(%s)", event.id, event.issuer->id, err,
                                 esp_err_to_name(err));
                    }
                }
            }
            if (event.payload.free_fcn) {
                event.payload.free_fcn(event.payload.data.ptr);
            }
        }
    }
}

//===============================================================================//
//================================= Public ======================================//
//===============================================================================//

esp_err_t
eventbus_init(size_t modules_amount) {
    EVENTBUS.modules_amount = modules_amount;
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    EVENTBUS.registry = calloc(EVENTBUS.modules_amount, sizeof(module_base*));
    if (!EVENTBUS.registry) {
        return ESP_ERR_NO_MEM;
    }
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

esp_err_t
eventbus_module_register(module_base* module, module_base_config_t* config) {
    esp_err_t err = ESP_OK;
    if (config->id < 0 || config->id > EVENTBUS.modules_amount) {
        ESP_LOGE(TAG, "unknown id %d", config->id);
        err = ESP_FAIL;
    } else if (EVENTBUS.registry[config->id]) {
        ESP_LOGE(TAG, "already exist id %d", config->id);
        err = ESP_ERR_INVALID_ARG;
    } else {
        err = module_create(module, config);
        if (err == ESP_OK) {
            EVENTBUS.registry[module->id] = module;
        }
    }
    return err;
}

module_base*
eventbus_module_get(int id) {
    if (id < 0 || id > EVENTBUS.modules_amount) {
        ESP_LOGE(TAG, "unknown id %d", id);
        return NULL;
    }
    return EVENTBUS.registry[id];
}

esp_err_t
eventbus_module_subscribe(module_base* self, int target_id, int evt_id) {
    module_base* addressee = eventbus_module_get(target_id);
    if (!addressee) {
        ESP_LOGE(TAG, "sub addressee not found for id %d", target_id);
        return ESP_FAIL;
    }
    if (evt_id > addressee->size) {
        ESP_LOGE(TAG, "unknown sub event id %d", evt_id);
        return ESP_FAIL;
    }
    subscription_t* new_sub = malloc(sizeof(subscription_t));
    if (!new_sub) {
        return ESP_ERR_NO_MEM;
    }
    new_sub->module = self;
    SLIST_INSERT_HEAD(&addressee->subscriptions[evt_id], new_sub, next);
    return ESP_OK;
}

esp_err_t
eventbus_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->id);
        return ESP_FAIL;
    }
    return ESP_OK;
}