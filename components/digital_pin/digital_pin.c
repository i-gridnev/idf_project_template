#include <esp_log.h>
#include <string.h>
#include "button_gpio.h"
#include "driver/gpio.h"
#include "iot_button.h"

#include "digital_pin.h"

#define TAG "DPIN"

typedef struct {
    module_base module;
    digital_pin_config_t config;
    button_handle_t btn;
} digital_pin_t;

static void
_input_event_cb(void* arg, void* data) {
    digital_pin_t* digital_pin = (digital_pin_t*)data;
    button_handle_t btn = (button_handle_t)arg;
    button_event_t evt = iot_button_get_event(btn);
    bool state = false;
    if (evt == BUTTON_PRESS_DOWN) {
        if (digital_pin->config.type == DIGITAL_PIN_TYPE_INPUT) {
            state = true;
        } else if (digital_pin->config.type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
            state = false;
        }
    } else if (evt == BUTTON_PRESS_END) {
        if (digital_pin->config.type == DIGITAL_PIN_TYPE_INPUT) {
            state = false;
        } else if (digital_pin->config.type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
            state = true;
        }
    }
    event_t event = {
        .id = EVT_DIGITAL_PIN_STATE_CLICK,
        .issuer = &digital_pin->module,
        .payload = {.data.b = state, .size = sizeof(bool), .free_fcn = NULL},
    };
    eventbus_post_event(&event);
}

static void
_button_event_cb(void* arg, void* data) {
    static uint8_t hold_repeat_counter;
    digital_pin_t* digital_pin = (digital_pin_t*)data;
    button_handle_t btn = (button_handle_t)arg;
    event_t event = {
        .issuer = &digital_pin->module,
        .payload.size = sizeof(int32_t),
        .payload.free_fcn = NULL,
    };
    button_event_t evt = iot_button_get_event(btn);
    if (evt == BUTTON_PRESS_REPEAT_DONE) {
        event.id = EVT_DIGITAL_PIN_STATE_CLICK;
        event.payload.data.i32 = iot_button_get_repeat(btn);
        eventbus_post_event(&event);
        return;
    } else if (evt == BUTTON_LONG_PRESS_START) {
        hold_repeat_counter = 1;
    } else if (evt == BUTTON_LONG_PRESS_HOLD) {
        hold_repeat_counter++;
    } else if (evt == BUTTON_LONG_PRESS_UP) {
        hold_repeat_counter = 0;
    }
    event.id = EVT_DIGITAL_PIN_LONG_LATCH;
    event.payload.data.double_u16.u1 = hold_repeat_counter;
    event.payload.data.double_u16.u2 = (uint16_t)iot_button_get_ticks_time(btn);
    eventbus_post_event(&event);
}

bool
digital_pin_get_state(module_base* digital_pin) {
    digital_pin_t* d_pin = (digital_pin_t*)digital_pin;
    bool state = false;
    if (d_pin->config.type == DIGITAL_PIN_TYPE_OUTPUT || d_pin->config.type == DIGITAL_PIN_TYPE_OUTPUT_INVERSE) {
        state = d_pin->config.opt.output.state;
    } else {
        state = iot_button_get_key_level(d_pin->btn);
        if (d_pin->config.type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
            state = !state;
        }
    }
    return state;
}

digital_pin_type
digital_pin_get_type(module_base* digital_pin) {
    digital_pin_t* d_pin = (digital_pin_t*)digital_pin;
    return d_pin->config.type;
}

esp_err_t
digital_pin_report_now(module_base* digital_pin) {
    event_t report_evt = {
        .id = EVT_DIGITAL_PIN_STATE_CLICK,
        .issuer = digital_pin,
        .payload = {.data.b = digital_pin_get_state(digital_pin), .size = sizeof(bool), .free_fcn = NULL},
    };
    return eventbus_post_event(&report_evt);
}

esp_err_t
digital_pin_set_silent(module_base* digital_pin, bool new_state) {
    esp_err_t err = ESP_OK;
    digital_pin_t* do_pin = (digital_pin_t*)digital_pin;

    if (do_pin->config.type != DIGITAL_PIN_TYPE_OUTPUT && do_pin->config.type != DIGITAL_PIN_TYPE_OUTPUT_INVERSE) {
        ESP_LOGE(TAG, "Invalid pin type");
        return ESP_FAIL;
    }
    if (do_pin->config.opt.output.state != new_state) {
        uint32_t new = do_pin->config.type == DIGITAL_PIN_TYPE_OUTPUT ? new_state : !new_state;
        esp_err_t err = gpio_set_level(do_pin->config.opt.output.gpio, new);
        if (err == ESP_OK) {
            do_pin->config.opt.output.state = new_state;
        } else {
            ESP_LOGE(TAG, "Failed gpio_set err=%d(%s)", err, esp_err_to_name(err));
        }
    }
    return err;
}

esp_err_t
digital_pin_set(module_base* digital_pin, bool new_state) {
    esp_err_t err = digital_pin_set_silent(digital_pin, new_state);
    if (err == ESP_OK) {
        event_t evt = {
            .id = EVT_DIGITAL_PIN_STATE_CLICK,
            .issuer = digital_pin,
            .payload = {.data.b = new_state, .size = sizeof(bool), .free_fcn = NULL},
        };
        err = eventbus_post_event(&evt);
    }
    return err;
}

esp_err_t
_init_output(digital_pin_t* digital_pin) {
    gpio_config_t pin_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << digital_pin->config.opt.output.gpio),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&pin_conf));
    return digital_pin_set(&digital_pin->module, digital_pin->config.opt.output.state);
}

esp_err_t
_init_input(digital_pin_t* digital_pin) {
    digital_pin_config_t* config = &digital_pin->config;

    button_config_t button_config = {0};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = config->opt.input.gpio,
        .enable_power_save = false,
        .active_level = config->opt.input.active_level,
        .disable_pull = config->opt.input.disable_pull,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_cfg, &digital_pin->btn));
    esp_err_t err = iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_DOWN, NULL, _input_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_END, NULL, _input_event_cb, digital_pin);
    return err;
}

esp_err_t
_init_button(digital_pin_t* digital_pin) {
    digital_pin_config_t* config = &digital_pin->config;

    button_config_t button_config = {
        .long_press_time = config->opt.button.long_press_time,
        .short_press_time = config->opt.button.short_press_time,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = config->opt.button.gpio,
        .enable_power_save = false,
        .active_level = config->opt.button.active_level,
        .disable_pull = config->opt.button.disable_pull,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_cfg, &digital_pin->btn));
    esp_err_t err =
        iot_button_register_cb(digital_pin->btn, BUTTON_PRESS_REPEAT_DONE, NULL, _button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_START, NULL, _button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_HOLD, NULL, _button_event_cb, digital_pin);
    err |= iot_button_register_cb(digital_pin->btn, BUTTON_LONG_PRESS_UP, NULL, _button_event_cb, digital_pin);
    return err;
}

module_base*
digital_pin_create(int id, digital_pin_config_t* config) {
    digital_pin_t* digital_pin = calloc(1, sizeof(digital_pin_t));
    memcpy(&digital_pin->config, config, sizeof(digital_pin_config_t));

    module_base_config_t base_config = {
        .id = id,
        .max_evts = EVT_DIGITAL_PIN_MAX,
        .event_handler = config->event_handler,
    };
    ESP_ERROR_CHECK(eventbus_module_register(&digital_pin->module, &base_config));

    if (config->type == DIGITAL_PIN_TYPE_INPUT || config->type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
        ESP_ERROR_CHECK(_init_input(digital_pin));
    } else if (config->type == DIGITAL_PIN_TYPE_BUTTON) {
        ESP_ERROR_CHECK(_init_button(digital_pin));
    } else if (config->type == DIGITAL_PIN_TYPE_OUTPUT || config->type == DIGITAL_PIN_TYPE_OUTPUT_INVERSE) {
        ESP_ERROR_CHECK(_init_output(digital_pin));
    } else {
        ESP_LOGE(TAG, "Unknown type");
        free(digital_pin);
        return NULL;
    }

    return &digital_pin->module;
}