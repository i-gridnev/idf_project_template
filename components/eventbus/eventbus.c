#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define TAG "EVENTBUS"

typedef struct {
    module_base* registry[MODULES_MAX];
    QueueHandle_t event_queue;
    TaskHandle_t event_task;
} eventbus_t;

static StaticQueue_t __eq_struct;
static uint8_t __eq_buf[EVT_QUEUE_SIZE * sizeof(event_t)];
static eventbus_t EVENTBUS;

static void
eventbus_task(void* params) {
    event_t event;

    while (true) {
        if (xQueueReceive(EVENTBUS.event_queue, &event, 1)) {
            subscription_t* sub;
            SLIST_FOREACH(sub, &event.issuer->subscriptions.on_evt[event.id], next) {
                sub->module->event_handler(event.issuer, &event);
            }
            if (event.payload.free_fcn) {
                event.payload.free_fcn(event.payload.data);
            }
        }
    }
}

void
eventbus_init() {
    EVENTBUS.event_queue = xQueueCreateStatic(EVT_QUEUE_SIZE, sizeof(event_t), __eq_buf, &__eq_struct);
    xTaskCreatePinnedToCore(eventbus_task, "ebus", 4096, NULL, EVT_TASK_PTIORITY, &EVENTBUS.event_task, EVT_TASK_CORE);
}

esp_err_t
eventbus_module_register(module_base* module) {
    if (module->id > MODULES_MAX) {
        ESP_LOGE(TAG, "bad id %d", module->id);
        return ESP_FAIL;
    }
    EVENTBUS.registry[module->id] = module;
    return ESP_OK;
}

module_base*
eventbus_module_get(int id) {
    if (id > MODULES_MAX) {
        ESP_LOGE(TAG, "bad id %d", id);
        return NULL;
    }
    return EVENTBUS.registry[id];
}

esp_err_t
eventbus_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->id);
        return ESP_FAIL;
    }
    return ESP_OK;
}