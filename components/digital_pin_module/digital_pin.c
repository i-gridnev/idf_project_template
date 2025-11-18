#include <esp_log.h>
#include <string.h>
#include "button_gpio.h"
#include "driver/gpio.h"

#include "digital_pin.h"

#define TAG "DPIN"

digital_pin_module_t DIGITAL_PIN_MODULE_obj = {
    MODULE_INIT(base, DIGITAL_PIN_MODULE),
};
digital_pin_module_t* DIGITAL_PIN_MODULE = &DIGITAL_PIN_MODULE_obj;

static void
_input_event_cb(void* arg, void* data) {
    digital_pin_t* pin = (digital_pin_t*)data;
    button_handle_t btn = (button_handle_t)arg;
    button_event_t evt = iot_button_get_event(btn);
    if (evt == BUTTON_PRESS_DOWN) {
        if (pin->config.type == DIGITAL_PIN_TYPE_INPUT) {
            pin->state = true;
        } else if (pin->config.type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
            pin->state = false;
        }
    } else if (evt == BUTTON_PRESS_END) {
        if (pin->config.type == DIGITAL_PIN_TYPE_INPUT) {
            pin->state = false;
        } else if (pin->config.type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
            pin->state = true;
        }
    }
    digital_pin_event_data_t evt_data = {
        .input = {.state = pin->state},
    };
    event_t event = {
        .id = EVT_DIGITAL_PIN_INPUT_CHANGE,
        .issuer = &pin->base,
        .data = evt_data.raw,
        .data_size = sizeof(digital_pin_event_data_t),
        .data_free_fcn = NULL,
    };
    device_post_event(&event);
}

static void
_button_event_cb(void* arg, void* data) {
    static uint16_t hold_repeat_counter;
    digital_pin_t* pin = (digital_pin_t*)data;
    button_handle_t btn = (button_handle_t)arg;
    event_t event = {
        .issuer = &pin->base,
        .data_size = sizeof(digital_pin_event_data_t),
        .data_free_fcn = NULL,
    };
    button_event_t evt = iot_button_get_event(btn);
    if (evt == BUTTON_PRESS_REPEAT_DONE) {
        uint16_t repeated_clicks = iot_button_get_repeat(btn);
        digital_pin_event_data_t evt_data = {
            .button = {.repeat_counter = repeated_clicks, .ticks_time = 0},
        };
        event.id = EVT_DIGITAL_PIN_BTN_CLICK;
        event.data = evt_data.raw;
        device_post_event(&event);
    } else {
        if (evt == BUTTON_LONG_PRESS_START) {
            pin->state = true;
            hold_repeat_counter = 1;
        } else if (evt == BUTTON_LONG_PRESS_HOLD) {
            hold_repeat_counter++;
        } else if (evt == BUTTON_LONG_PRESS_UP) {
            pin->state = false;
            hold_repeat_counter = 0;
        }
        event.id = EVT_DIGITAL_PIN_BTN_LONG_LATCH;
        digital_pin_event_data_t evt_data = {
            .button = {.repeat_counter = hold_repeat_counter, .ticks_time = iot_button_get_ticks_time(btn)},
        };
        event.data = evt_data.raw;
        device_post_event(&event);
    }
}

esp_err_t
digital_pin_set_silent(digital_pin_t* pin, bool new_state) {
    esp_err_t err = ESP_OK;
    if (pin->config.type != DIGITAL_PIN_TYPE_OUTPUT && pin->config.type != DIGITAL_PIN_TYPE_OUTPUT_INVERSE) {
        ESP_LOGE(TAG, "Invalid pin type");
        return ESP_FAIL;
    }
    if (pin->state != new_state) {
        uint32_t new = pin->config.type == DIGITAL_PIN_TYPE_OUTPUT ? new_state : !new_state;
        esp_err_t err = gpio_set_level(pin->config.gpio, new);
        if (err == ESP_OK) {
            pin->state = new_state;
        } else {
            ESP_LOGE(TAG, "Failed gpio_set err=%d(%s)", err, esp_err_to_name(err));
        }
    }
    return err;
}

esp_err_t
digital_pin_set(digital_pin_t* pin, bool new_state) {
    esp_err_t err = digital_pin_set_silent(pin, new_state);
    if (err == ESP_OK) {
        digital_pin_event_data_t evt_data = {
            .output = {.state = new_state},
        };
        event_t evt = {
            .issuer = &pin->base,
            .id = EVT_DIGITAL_PIN_OUTPUT_CHANGE,
            .data = evt_data.raw,
            .data_size = sizeof(digital_pin_event_data_t),
            .data_free_fcn = NULL,
        };
        err = device_post_event(&evt);
    }
    return err;
}

esp_err_t
_init_output(digital_pin_t* pin) {
    gpio_config_t pin_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << pin->config.gpio),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&pin_conf));
    return digital_pin_set(pin, pin->config.opt.output.init_state);
}

esp_err_t
_init_input(digital_pin_t* pin) {
    digital_pin_config_t* config = &pin->config;

    button_config_t button_config = {0};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = config->gpio,
        .enable_power_save = false,
        .active_level = config->opt.input.active_level,
        .disable_pull = config->opt.input.disable_pull,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_cfg, &pin->_btn));
    esp_err_t err = iot_button_register_cb(pin->_btn, BUTTON_PRESS_DOWN, NULL, _input_event_cb, pin);
    err |= iot_button_register_cb(pin->_btn, BUTTON_PRESS_END, NULL, _input_event_cb, pin);
    return err;
}

esp_err_t
_init_button(digital_pin_t* pin) {
    digital_pin_config_t* config = &pin->config;

    button_config_t button_config = {
        .long_press_time = config->opt.button.long_press_time,
        .short_press_time = config->opt.button.short_press_time,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = config->gpio,
        .enable_power_save = false,
        .active_level = config->opt.button.active_level,
        .disable_pull = config->opt.button.disable_pull,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_cfg, &pin->_btn));
    esp_err_t err = iot_button_register_cb(pin->_btn, BUTTON_PRESS_REPEAT_DONE, NULL, _button_event_cb, pin);
    err |= iot_button_register_cb(pin->_btn, BUTTON_LONG_PRESS_START, NULL, _button_event_cb, pin);
    err |= iot_button_register_cb(pin->_btn, BUTTON_LONG_PRESS_HOLD, NULL, _button_event_cb, pin);
    err |= iot_button_register_cb(pin->_btn, BUTTON_LONG_PRESS_UP, NULL, _button_event_cb, pin);
    return err;
}

digital_pin_t*
digital_pin_create(int id, digital_pin_config_t* config) {
    digital_pin_t* pin = calloc(1, sizeof(digital_pin_t));
    memcpy(&pin->config, config, sizeof(digital_pin_config_t));

    device_instance_constructor(&pin->base, &DIGITAL_PIN_MODULE->base, id);
    esp_err_t err = device_module_add_instance(&DIGITAL_PIN_MODULE->base, &pin->base);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to create instance id=%d for %s err=%d(%s)", id, DIGITAL_PIN_MODULE->base.name, err,
                 esp_err_to_name(err));
        free(pin);
        return NULL;
    }

    if (config->type == DIGITAL_PIN_TYPE_INPUT || config->type == DIGITAL_PIN_TYPE_INPUT_INVERSE) {
        ESP_ERROR_CHECK(_init_input(pin));
    } else if (config->type == DIGITAL_PIN_TYPE_BUTTON) {
        ESP_ERROR_CHECK(_init_button(pin));
    } else if (config->type == DIGITAL_PIN_TYPE_OUTPUT || config->type == DIGITAL_PIN_TYPE_OUTPUT_INVERSE) {
        ESP_ERROR_CHECK(_init_output(pin));
    } else {
        ESP_LOGE(TAG, "Unknown pin type");
        free(pin);
        return NULL;
    }

    return pin;
}