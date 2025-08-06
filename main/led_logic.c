#include <config_storage.h>
#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>

#include <digital_pin.h>
#include <led_logic.h>

#define TAG "LED_LOG"

esp_err_t
led_logic_handler(module_base* self, event_t* event) {
    esp_err_t err = ESP_OK;
    if (event->id == EVT_DIGITAL_PIN_STATE_CLICK) {
        uint16_t clicks = event->payload.data.i32;
        if (clicks == 1) {
            led_state_t l1 = {.opt.index = 2, .opt.red = 255, .opt.green = 0, .opt.blue = 0};
            err = ws_ledstrip_set_led_blink(self, l1, 200, 200, true);
        } else if (clicks == 2) {
            led_state_t l2 = {.opt.index = 3, .opt.red = 0, .opt.green = 255, .opt.blue = 0};
            err = ws_ledstrip_set_led_blink(self, l2, 200, 400, true);
        } else if (clicks == 3) {
            err = ws_ledstrip_reset_all(self);
        }
    }
    return err;
}

esp_err_t
led_logic() {
    ws_ledstrip_config_t config = {
        .gpio = 1, .leds_amount = 5, .invert_out = false, .with_dma = false, .event_handler = led_logic_handler};
    module_base* strip = ws_ledstrip_create(MODULE_LED_STRIP, &config);
    return eventbus_module_subscribe(strip, MODULE_DI_BTN, EVT_DIGITAL_PIN_STATE_CLICK);
}