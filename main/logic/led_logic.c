#include <esp_log.h>

#include <device_config.h>
#include <eventbus.h>

#define TAG "LED_LOG"

esp_err_t
led_logic() {
    esp_err_t err = ESP_FAIL;

    ws_strip_t* strip = ws_ledstrip_create(1, 5, false, false);
    if (strip == NULL) {
        return err;
    }
    for (int i = LED_WIFI_SMART; i < LED_MAX; i++) {
        ws_led_t* led = ws_led_create(i, strip, i);
        if (led == NULL) {
            return err;
        }
    }
    return ESP_OK;
}