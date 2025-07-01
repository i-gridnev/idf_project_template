#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define TAG "EVENTBUS"

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
eventbus_module_register(module_base* module) {
    if (module->id < 0 || module->id > EVENTBUS.modules_amount) {
        ESP_LOGE(TAG, "unknown id %d", module->id);
        return ESP_FAIL;
    }
    if (EVENTBUS.registry[module->id]) {
        ESP_LOGE(TAG, "already exist id %d", module->id);
        return ESP_FAIL;
    }
    EVENTBUS.registry[module->id] = module;
    return ESP_OK;
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
eventbus_post_event(event_t* event) {
    if (!xQueueSend(EVENTBUS.event_queue, event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: id=%d, issuer_id=%d", event->id, event->issuer->id);
        return ESP_FAIL;
    }
    return ESP_OK;
}