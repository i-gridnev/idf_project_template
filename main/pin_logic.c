#include <config_storage.h>
#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>
#include <wifi_network.h>

#include <pin_logic.h>

#define TAG "PIN"

esp_err_t
pin_logic_handler(module_base* self, event_t* event) {
    esp_err_t err = ESP_OK;
    if (event->id == EVT_DIGITAL_PIN_STATE_CLICK) {
        uint16_t clicks = event->payload.data.i32;
        ESP_LOGI(TAG, "Click %d", clicks);
        // if (clicks == 2) {
        //     bool is_off = is_wifi_network_STA_connected();
        //     if (!is_off) {
        //         err = wifi_network_STA_connect(CONFIG[CONFIG_WIFI_SSID].value.data.str,
        //                                        CONFIG[CONFIG_WIFI_PASSWORD].value.data.str);
        //     } else {
        //         err = wifi_network_STA_disconnect();
        //     }
        // }
    } else if (event->id == EVT_DIGITAL_PIN_LONG_LATCH) {
        uint16_t hold_num = event->payload.data.double_u16.u1;
        uint16_t ticks = event->payload.data.double_u16.u2;
        if (hold_num == 1) {
            ESP_LOGI(TAG, "Hold started");
        } else if (hold_num == 0) {
            ESP_LOGI(TAG, "Hold finished after %d ms", ticks);
        } else {
            ESP_LOGI(TAG, "Holding %d", hold_num - 1);
        }
    }
    return err;
}

esp_err_t
pin_logic() {
    digital_pin_config_t btn_cnf = {
        .type = DIGITAL_PIN_TYPE_BUTTON,
        .event_handler = pin_logic_handler,
        .opt = {.button = {.gpio = 0,
                           .active_level = 0,
                           .disable_pull = false,
                           .long_press_time = 1000,
                           .short_press_time = 180}},

    };
    module_base* b1 = digital_pin_create(MODULE_DI_BTN, &btn_cnf);
    esp_err_t err = eventbus_module_subscribe(b1, b1->id, EVT_DIGITAL_PIN_STATE_CLICK);
    err |= eventbus_module_subscribe(b1, b1->id, EVT_DIGITAL_PIN_LONG_LATCH);
    return err;
}