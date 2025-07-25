#include "esp_log.h"
#include "eventbus.h"

#define TAG "MODULE"

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

esp_err_t
module_subscribe(module_base* self, int target_id, int evt_id) {
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