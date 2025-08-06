#ifndef _WS_LEDSTRIP_H_
#define _WS_LEDSTRIP_H_

#include "esp_err.h"

#include "eventbus.h"

typedef union {
    int _raw_value;

    struct {
        uint8_t index;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } opt;
} led_state_t;

typedef struct {
    int32_t gpio;
    size_t leds_amount;
    bool with_dma;
    bool invert_out;
    event_handler event_handler;
} ws_ledstrip_config_t;

module_base* ws_ledstrip_create(int id, ws_ledstrip_config_t* config);

esp_err_t ws_ledstrip_set_led(module_base* self, led_state_t state);

esp_err_t ws_ledstrip_set_led_blink(module_base* self, led_state_t state, int on_ms, int off_ms, bool start_with);

esp_err_t ws_ledstrip_reset_all(module_base* self);

#endif /* _WS_LEDSTRIP_H_ */