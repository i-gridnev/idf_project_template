#include <esp_log.h>
#include <string.h>
#include "button_gpio.h"
#include "iot_button.h"

#include "digital_pin.h"
#include "eventbus.h"

#define TAG "DPIN"

typedef struct {
    module_base module;
    digital_pin_config_t config;
    button_handle_t btn;
    bool current_state;
} digital_pin_t;

static void
button_event_cb(void* arg, void* data) {
    static uint8_t hold_repeat_counter;
    digital_pin_t* digital_pin = (digital_pin_t*)data;
    button_handle_t btn = (button_handle_t)arg;

    // iot_button_print_event(btn);
    button_event_t evt = iot_button_get_event(btn);
    if (evt == BUTTON_PRESS_DOWN) {
        if (!digital_pin->current_state) {
            digital_pin->current_state = true;
            ESP_LOGI(TAG, "ON");
        }
    } else if (evt == BUTTON_LONG_PRESS_START) {
        hold_repeat_counter = 1;
        ESP_LOGI(TAG, "Click Long cnt=%d", hold_repeat_counter);
    } else if (evt == BUTTON_LONG_PRESS_HOLD) {
        uint32_t ticks = iot_button_get_ticks_time(btn) - hold_repeat_counter * digital_pin->config.long_press_time;
        if (ticks >= digital_pin->config.long_press_time) {
            hold_repeat_counter++;
            ESP_LOGI(TAG, "Click Long cnt=%d", hold_repeat_counter);
        }
    } else if (evt == BUTTON_LONG_PRESS_UP) {
        hold_repeat_counter = 0;
        uint32_t ticks = iot_button_get_ticks_time(btn);
        ESP_LOGI(TAG, "Click Long ended ticks=%lu", ticks);
    } else if (evt == BUTTON_PRESS_END) {
        if (digital_pin->current_state) {
            digital_pin->current_state = false;
            ESP_LOGI(TAG, "OFF");
        }
    } else if (evt == BUTTON_PRESS_REPEAT_DONE) {
        uint8_t cnt = iot_button_get_repeat(btn);
        ESP_LOGI(TAG, "Click %d times", cnt);
    }
}

esp_err_t
test_event_handler(module_base* self, event_t* event) {
    return ESP_OK;
}

void
digital_pin_create(int id, digital_pin_config_t* config) {
    digital_pin_t* digital_pin = calloc(1, sizeof(digital_pin_t));
    memcpy(&digital_pin->config, config, sizeof(digital_pin_config_t));
    digital_pin->current_state = config->init_state;

    button_config_t button_config = {
        .long_press_time = config->long_press_time,
        .short_press_time = config->short_press_time,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = config->gpio,
        .active_level = config->active_level,
        .enable_power_save = false,
        .disable_pull = false,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_cfg, &digital_pin->btn));
    esp_err_t err = iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_UP, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_REPEAT, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_REPEAT_DONE, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_START, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_UP, NULL, button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_END, NULL, button_event_cb, digital_pin);
    ESP_ERROR_CHECK(err);

    module_base_config_t base_config = {
        .id = id,
        .max_evts = EVT_DIGITAL_PIN_MAX,
        .event_handler = test_event_handler,
    };
    ESP_ERROR_CHECK(module_create(&digital_pin->module, &base_config));
    ESP_ERROR_CHECK(eventbus_module_register(&digital_pin->module));
    for (int i = 0; i < EVT_DIGITAL_PIN_MAX; i++) {
        module_subscribe(&digital_pin->module, id, i); // Subscribe on itself
    }
}