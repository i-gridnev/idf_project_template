#include <config_entry.h>
#include <device_config.h>
#include <esp_log.h>
#include <eventbus.h>
#include <wifi_network.h>

#include <pin_logic.h>

#define TAG "PIN"

esp_err_t
pin_handler(instance_base_t* subscriber, event_t* event) {
    esp_err_t err = ESP_OK;
    digital_pin_event_data_t btn_data = (digital_pin_event_data_t)event->data;
    if (event->id == EVT_DIGITAL_PIN_BTN_CLICK) {
        ESP_LOGI(TAG, "Click %d", btn_data.button.repeat_counter);
        // if (clicks == 2) {
        //     bool is_off = is_wifi_network_STA_connected();
        //     if (!is_off) {
        //         err = wifi_network_STA_connect(CONFIG[CONFIG_WIFI_SSID].value.data.str,
        //                                        CONFIG[CONFIG_WIFI_PASSWORD].value.data.str);
        //     } else {
        //         err = wifi_network_STA_disconnect();
        //     }
        // }
    } else if (event->id == EVT_DIGITAL_PIN_BTN_LONG_LATCH) {
        uint16_t hold_num = btn_data.button.repeat_counter;
        uint16_t ticks = btn_data.button.ticks_time;
        if (hold_num == 1) {
            ESP_LOGI(TAG, "Hold started");
        } else if (hold_num == 0) {
            ESP_LOGI(TAG, "Hold finished after %d ms", ticks);
        } else {
            ESP_LOGI(TAG, "Holding x%d for %d ms", hold_num - 1, ticks);
        }
    }
    return err;
}

esp_err_t
pin_logic() {
    esp_err_t err = ESP_OK;
    digital_pin_config_t btn_cfg = {
        .type = DIGITAL_PIN_TYPE_BUTTON,
        .gpio = 0,
        .opt = {.button = {.active_level = 0, .disable_pull = false, .long_press_time = 1000, .short_press_time = 180}},
    };
    digital_pin_t* btn_pin = digital_pin_create(PIN_BTN, &btn_cfg);
    err = device_subscribe(&btn_pin->base, &btn_pin->base, EVT_DIGITAL_PIN_BTN_CLICK, pin_handler);
    err |= device_subscribe(&btn_pin->base, &btn_pin->base, EVT_DIGITAL_PIN_BTN_LONG_LATCH, pin_handler);
    return err;
}