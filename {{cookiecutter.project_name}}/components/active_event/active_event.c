#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"

#include "active_event.h"
#include "esp_heap_trace.h"

#define TAG               "AE"

#define EVT_QUEUE_DEPTH   50
#define EVT_TASK_PTIORITY 5

typedef struct {
    evt_sign_t sign;
    value_t data;
    clear_data_f clear_data;
} evt_t;

typedef struct {
    evt_sign_t sign;
    evt_handler_t handler;
} evt_data_subscriprion_t;

typedef struct {
    int h_amount;
    evt_handler_t* handlers;
} listener_t;

typedef struct {
    QueueHandle_t queue;
    listener_t listeners[AEVT_MAX_EVT];
} active_event_obj_t;

static active_event_obj_t AE;

static void
ae_task(void* params) {
    esp_err_t err;
    evt_t event;
    listener_t* listener;

    while (true) {
        if (xQueueReceive(AE.queue, &event, 1)) {
            err = ESP_ERR_NOT_FOUND;
            listener = &AE.listeners[event.sign];
            for (int i = 0; i < listener->h_amount; i++) {
                err = (*listener->handlers[i])(event.sign, event.data);
            }
            if (err == ESP_ERR_NOT_FOUND) {
                ESP_LOGW(TAG, "unhandled evt_sign=%d", event.sign);
            }
            if (event.clear_data) {
                (*event.clear_data)(&event.data);
            }
            memset(&event, 0, sizeof(evt_t));
            ESP_LOGI(TAG, "RAM: free=%lu, min=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
        }
    }
}

static int
find_handler_index(listener_t* listener, evt_handler_t handler) {
    for (int i = 0; i < listener->h_amount; i++) {
        if (listener->handlers[i] == handler) {
            return i;
        }
    }
    return -1;
}

static esp_err_t
unsubscribe_handler(evt_sign_t sign, value_t data) {
    evt_data_subscriprion_t* subscr = (evt_data_subscriprion_t*)data.ptr;
    listener_t* listener = &AE.listeners[subscr->sign];
    if (!listener->h_amount) {
        ESP_LOGE(TAG, "UNSUB evt_sign=%d no handlers exist", subscr->sign);
        return ESP_FAIL;
    }

    int h_id = find_handler_index(listener, subscr->handler);
    if (h_id < 0) {
        ESP_LOGE(TAG, "UNSUB evt_sign=%d handler not found", subscr->sign);
        return ESP_FAIL;
    }

    if (h_id != listener->h_amount - 1) { //not last item => shifting
        for (int j = h_id; j < listener->h_amount - 1; j++) {
            listener->handlers[j] = listener->handlers[j + 1];
        }
    }
    evt_handler_t* new_h = realloc(listener->handlers, (listener->h_amount - 1) * sizeof(evt_handler_t));
    if (!new_h && listener->h_amount != 1) {
        ESP_LOGE(TAG, "UNSUB evt_sign=%d handlers memory error", subscr->sign);
        return ESP_FAIL;
    }
    listener->handlers = new_h;
    listener->h_amount--;

    ESP_LOGW(TAG, "UNSUB evt_sign=%d", subscr->sign);
    return ESP_OK;
}

static esp_err_t
subscribe_handler(evt_sign_t sign, value_t data) {
    evt_data_subscriprion_t* subscr = (evt_data_subscriprion_t*)data.ptr;
    listener_t* listener = &AE.listeners[subscr->sign];

    int h_id = find_handler_index(listener, subscr->handler);
    if (h_id >= 0) {
        ESP_LOGE(TAG, "SUB evt_sign=%d handler already exist", subscr->sign);
        return ESP_FAIL;
    }

    evt_handler_t* new_h = realloc(listener->handlers, (listener->h_amount + 1) * sizeof(evt_handler_t));
    if (!new_h) {
        ESP_LOGE(TAG, "SUB evt_sign=%d handlers memory error", subscr->sign);
        return ESP_FAIL;
    }

    listener->handlers = new_h;
    listener->handlers[listener->h_amount] = subscr->handler;
    listener->h_amount++;
    ESP_LOGW(TAG, "SUB on evt_sign=%d", subscr->sign);
    return ESP_OK;
}

static esp_err_t
subscr_event(evt_sign_t evt_sign, evt_handler_t handler, evt_sign_t action) {
    if (evt_sign >= AEVT_MAX_EVT || evt_sign < 0) {
        ESP_LOGE(TAG, "unknown evt_sign=%d", evt_sign);
        return ESP_FAIL;
    }

    evt_data_subscriprion_t* subscr = calloc(1, sizeof(evt_data_subscriprion_t));
    subscr->sign = evt_sign;
    subscr->handler = handler;
    value_t evt_data = {.ptr = subscr};

    return ae_post_evt(action, evt_data, &ae_free_pointer);
}

void
ae_free_pointer(value_t* data) {
    free(data->ptr);
}

esp_err_t
ae_subscribe(evt_sign_t evt_sign, evt_handler_t handler) {
    return subscr_event(evt_sign, handler, AEVT_SUBSCRIBE);
}

esp_err_t
ae_unsubscribe(evt_sign_t evt_sign, evt_handler_t handler) {
    return subscr_event(evt_sign, handler, AEVT_UNSUBSCRIBE);
}

esp_err_t
ae_post_evt(evt_sign_t sign, value_t data, clear_data_f free_fcn) {
    evt_t event = {.sign = sign, .data = data, .clear_data = free_fcn};
    if (!xQueueSend(AE.queue, &event, 0)) {
        ESP_LOGE(TAG, "failed to post evt: sign=%d, data_int=%d", event.sign, event.data.integer);
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool
ae_post_evt_FromISR(evt_sign_t sign, value_t data, clear_data_f free_fcn) {
    evt_t event = {.sign = sign, .data = data, .clear_data = free_fcn};
    BaseType_t high_task_wakeup;
    xQueueSendFromISR(AE.queue, &event, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

bool
ae_post_evt_CRITICAL_FromISR(evt_sign_t sign, value_t data, clear_data_f free_fcn) {
    evt_t event = {.sign = sign, .data = data, .clear_data = free_fcn};
    BaseType_t high_task_wakeup;
    xQueueSendToBackFromISR(AE.queue, &event, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

esp_err_t
ae_init() {
    listener_t* listener;
    ESP_LOGI(TAG, "Sizes: evt_sign_t:%d, evt_t=%d, AE=%d", sizeof(evt_sign_t), sizeof(evt_t), sizeof(AE));

    AE.queue = xQueueCreate(EVT_QUEUE_DEPTH, sizeof(evt_t));
    if (!AE.queue) {
        ESP_LOGE(TAG, "Queue create failed");
        return ESP_FAIL;
    }

    listener = &AE.listeners[AEVT_SUBSCRIBE];
    listener->handlers = malloc(sizeof(evt_handler_t));
    (*listener->handlers) = &subscribe_handler;
    listener->h_amount = 1;

    listener = &AE.listeners[AEVT_UNSUBSCRIBE];
    listener->handlers = malloc(sizeof(evt_handler_t));
    (*listener->handlers) = &unsubscribe_handler;
    listener->h_amount = 1;

    xTaskCreatePinnedToCore(ae_task, "ae_task", 4096, NULL, EVT_TASK_PTIORITY, NULL, 1);
    return ESP_OK;
}
