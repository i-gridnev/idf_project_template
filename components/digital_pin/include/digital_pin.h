#ifndef _DIGITAL_PIN_H_
#define _DIGITAL_PIN_H_

#include "esp_err.h"

enum {
    EVT_DI_ON,
    EVT_DIGITAL_PIN_MAX,
};

typedef struct {
    int32_t gpio;
    uint8_t active_level; /**< gpio level when press down */
    bool disable_pull;    /**< disable internal pull up or down */
    uint16_t long_press_time;
    uint16_t short_press_time;
    bool init_state;
} digital_pin_config_t;

void digital_pin_create(int id, digital_pin_config_t* config);

#endif /* _DIGITAL_PIN_H_ */