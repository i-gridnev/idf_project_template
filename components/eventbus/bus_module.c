#include "esp_log.h"
#include "eventbus.h"

#define TAG "MODULE"

esp_err_t
module_create(module_base* module, module_base_config_t* config) {
    module->id = config->id;

    module->subscriptions.size = config->max_evts;
    module->subscriptions.on_evt = malloc(module->subscriptions.size * sizeof(struct subscription_head));
    if (!module->subscriptions.on_evt) {
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < module->subscriptions.size; i++) {
        SLIST_INIT(&module->subscriptions.on_evt[i]);
    };
    module->event_handler = config->event_handler;
    return ESP_OK;
}

esp_err_t
module_subscribe(module_base* module, int target_id, int evt_id) {
    module_base* addressee = eventbus_module_get(target_id);
    if (!addressee) {
        ESP_LOGE(TAG, "sub addressee not found for id %d", target_id);
        return ESP_FAIL;
    }
    if (evt_id > addressee->subscriptions.size) {
        ESP_LOGE(TAG, "bad sub event id %d", evt_id);
        return ESP_FAIL;
    }
    subscription_t* new_sub = malloc(sizeof(subscription_t));
    if (!new_sub) {
        return ESP_ERR_NO_MEM;
    }
    new_sub->module = module;
    SLIST_INSERT_HEAD(&addressee->subscriptions.on_evt[evt_id], new_sub, next);
    return ESP_OK;
}