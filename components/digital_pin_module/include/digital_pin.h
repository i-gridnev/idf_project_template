/*
 * Component: Digital PIN
 * 
 * Module for managing MCU pins with digital signals. Provide instance of Button, Input or Output entity.
*/

#ifndef _DIGITAL_PIN_H_
#define _DIGITAL_PIN_H_

#include "esp_err.h"
#include "iot_button.h"

#include "eventbus.h"

DEVICE_MODULE_DECLARE(DIGITAL_PIN_MODULE);

// typedef struct {
//     module_base_t base;
// } digital_pin_module_t;

// extern digital_pin_module_t* DIGITAL_PIN_MODULE;

//=============================================================//
//============= EVENT DESCRIPTION =============================//
enum {
    EVT_DIGITAL_PIN_BTN_CLICK,
    EVT_DIGITAL_PIN_BTN_LONG_LATCH,
    EVT_DIGITAL_PIN_INPUT_CHANGE,
    EVT_DIGITAL_PIN_OUTPUT_CHANGE,
    EVT_DIGITAL_PIN_MAX,
};

typedef union {
    void* raw;

    struct {
        uint16_t repeat_counter;
        uint16_t ticks_time;
    } button;

    struct {
        bool state;
    } input;

    struct {
        bool state;
    } output;
} digital_pin_event_data_t;

//=========================================================//
//==================== PIN ENTITY =========================//
typedef enum {
    DIGITAL_PIN_TYPE_INPUT,
    DIGITAL_PIN_TYPE_INPUT_INVERSE,
    DIGITAL_PIN_TYPE_BUTTON,
    DIGITAL_PIN_TYPE_OUTPUT,
    DIGITAL_PIN_TYPE_OUTPUT_INVERSE,
} digital_pin_type;

typedef struct {
    digital_pin_type type;
    int32_t gpio;

    union {
        struct {
            uint8_t active_level; /**< gpio level when press down */
            bool disable_pull;    /**< disable internal pull up or down */
            uint16_t long_press_time;
            uint16_t short_press_time;
        } button;

        struct {
            uint8_t active_level; /**< gpio level when press down */
            bool disable_pull;    /**< disable internal pull up or down */
        } input;

        struct {
            bool init_state;
        } output;
    } opt;
} digital_pin_config_t;

typedef struct {
    component_base_t base;
    bool state;
    digital_pin_config_t config;
    button_handle_t _btn;
} digital_pin_t;

//=========================================================//

digital_pin_t* digital_pin_create(int id, digital_pin_config_t* config);

esp_err_t digital_pin_set(digital_pin_t* pin, bool new_state);

esp_err_t digital_pin_set_silent(digital_pin_t* pin, bool new_state);

#endif /* _DIGITAL_PIN_H_ */