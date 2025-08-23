#ifndef _WS_LEDSTRIP_H_
#define _WS_LEDSTRIP_H_

#include "esp_err.h"

#include "eventbus.h"

typedef enum { LED_OFF = 0, LED_ON, LED_BLINK, LED_FADE } led_status_t;

typedef enum {
    LED_CMD_OFF,
    LED_CMD_SET_RGB,
    LED_CMD_BLINK,
    LED_CMD_FADE,
    /// System, use with caution
    LED_CMD_CLEAR_ALL,
    LED_CMD_TIMER_TICK,
    LED_CMD_MAX,
} led_cmd_type_t;

typedef union {
    int _raw_value;

    struct state_opt {
        uint8_t status;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } opt;
} led_state_t;

typedef struct {
    uint16_t strip_id;
    uint16_t led_id;
    led_cmd_type_t type;
    led_state_t target_state;

    union {
        struct blink {
            uint16_t on_ms;
            uint16_t off_ms;
            uint16_t repeat;
        } blink;

        struct fade {
            uint16_t duration;
        } fade;
    } cmd_opt;
} led_cmd_t;

typedef struct {
    int32_t gpio;
    size_t leds_amount;
    bool with_dma;
    bool invert_out;
} ws_ledstrip_config_t;

typedef struct {
    size_t strips_amount;
    event_handler event_handler;
} ws_ledstrip_manager_config_t;

module_base* ws_ledstrip_manager_create(int id, ws_ledstrip_manager_config_t* config);

esp_err_t ws_ledstrip_add_strip(int strip_id, ws_ledstrip_config_t* config);

esp_err_t ws_ledstrip_send_cmd(led_cmd_t* cmd);

led_state_t ws_ledstrip_get_state(int strip_id, int led_id);

#endif /* _WS_LEDSTRIP_H_ */