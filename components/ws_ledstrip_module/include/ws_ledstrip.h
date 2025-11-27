/*
 * Component: Led Strip (WS2812)
 * 
*/

#ifndef _WS_LEDSTRIP_H_
#define _WS_LEDSTRIP_H_

#include <led_strip.h>
#include "esp_err.h"
#include "eventbus.h"

DEVICE_MODULE_DECLARE(LEDSTRIP_MODULE);

//=============================================================//
//============= LED ENTITY ====================================//

typedef enum {
    LED_STATE_DISABLED = -1,
    LED_STATE_OFF,
    LED_STATE_ON,
    LED_STATE_BLINK,
    LED_STATE_BLINK_REPEAT,
    LED_STATE_FADE,
    LED_STATE_MAX,
} led_status_e;

typedef union {
    struct blink {
        uint16_t on_ms;
        uint16_t off_ms;
    } blink;

    struct blink_repeat {
        uint16_t on_ms;
        uint16_t off_ms;
        uint8_t repeat;
        uint16_t repeat_delay_ms;
    } blink_repeat;

    struct fade {
        uint16_t duration;
    } fade;
} led_status_opt_t;

typedef struct ws_strip* ws_strip_t;

typedef struct {
    component_base_t base;
    ws_strip_t strip;
    int pos_index;
    int tick_counter;
    int repeat_counter;

    bool _on;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    led_status_e prev_status;
    led_status_e status;
    led_status_opt_t status_opt;
} ws_led_t;

ws_strip_t ws_ledstrip_create(int32_t gpio, uint32_t max_leds, bool with_dma, bool invert_out);

ws_led_t* ws_led_create(int id, ws_strip_t stripe, int pos_index);

// led_status_t ws_led_get_status(int led_id);

esp_err_t ws_led_set(int led_id, led_status_e status, uint8_t r, uint8_t g, uint8_t b, led_status_opt_t* opt);

#endif /* _WS_LEDSTRIP_H_ */