/*
 * Component: Led Strip (WS2812)
 * 
*/

#ifndef _WS_LEDSTRIP_H_
#define _WS_LEDSTRIP_H_

#include <led_strip.h>
#include "esp_err.h"
#include "eventbus.h"

typedef struct {
    module_base_t base;
} ledstrip_module_t;

extern ledstrip_module_t* LEDSTRIP_MODULE;

//=============================================================//
//============= LED ENTITY ====================================//

typedef enum {
    LED_STATE_OFF,
    LED_STATE_ON,
    LED_STATE_BLINK,
    LED_STATE_FADE,
    LED_STATE_MAX,
} led_state_t;

typedef union {
    int _raw_value;

    struct status_opt {
        bool _on;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } opt;
} led_status_t;

typedef union {
    struct blink {
        uint16_t on_ms;
        uint16_t off_ms;
        uint16_t repeat;
    } blink;

    struct fade {
        uint16_t duration;
    } fade;
} led_status_opt_t;

typedef struct ws_strip* ws_strip_t;

typedef struct {
    instance_base_t base;
    ws_strip_t strip;
    int tick_counter;
    led_state_t state;
    led_status_t status;
    led_status_opt_t status_opt;
} ws_led_t;

ws_strip_t ws_ledstrip_create(int id, int32_t gpio, uint32_t max_leds, bool with_dma, bool invert_out);

ws_led_t* ws_led_create(int id, ws_strip_t stripe, int index_position);

led_status_t ws_led_get_status(int led_id);

esp_err_t ws_led_set(ws_led_t* led, led_state_t state, led_status_t status, led_status_opt_t* opt);

// esp_err_t ws_ledstrip_send_cmd(led_cmd_t* cmd);

#endif /* _WS_LEDSTRIP_H_ */