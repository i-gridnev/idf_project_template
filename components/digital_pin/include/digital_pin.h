#ifndef _DIGITAL_PIN_H_
#define _DIGITAL_PIN_H_

#include "esp_err.h"

#include "eventbus.h"

enum {
    EVT_DIGITAL_PIN_STATE_CLICK,
    EVT_DIGITAL_PIN_LONG_LATCH,
    EVT_DIGITAL_PIN_MAX,
};

typedef enum {
    DIGITAL_PIN_TYPE_INPUT,
    DIGITAL_PIN_TYPE_INPUT_INVERSE,
    DIGITAL_PIN_TYPE_BUTTON,
    DIGITAL_PIN_TYPE_OUTPUT,
    DIGITAL_PIN_TYPE_OUTPUT_INVERSE,
} digital_pin_type;

typedef union {
    struct {
        int32_t gpio;
        uint8_t active_level; /**< gpio level when press down */
        bool disable_pull;    /**< disable internal pull up or down */
        uint16_t long_press_time;
        uint16_t short_press_time;
    } button;

    struct {
        int32_t gpio;
        uint8_t active_level; /**< gpio level when press down */
        bool disable_pull;    /**< disable internal pull up or down */
    } input;

    struct {
        int32_t gpio;
        bool state;
    } output;
} digital_pin_config_t;

module_base* digital_pin_create(int id, digital_pin_type type, digital_pin_config_t* config);

esp_err_t digital_pin_report_now(module_base* digital_pin);

bool digital_pin_get(module_base* digital_pin);

esp_err_t digital_pin_set(module_base* digital_pin, bool new_state);

esp_err_t digital_pin_set_silent(module_base* digital_pin, bool new_state);

#endif /* _DIGITAL_PIN_H_ */